#include <BandPassComputeComplex.hpp>
#include <VariablesResolve.hpp>

#include <CpuComplexSignal.hpp>
#include <CpuFloatSignal.hpp>
#include <EmptyContainer.hpp>
#include <module.hpp>

#include <cmath>
#include <memory>

namespace {

double sinc(double x) {
  if (std::abs(x) < 1.0e-12) {
    return 1.0;
  }
  return std::sin(M_PI * x) / (M_PI * x);
}

double hammingWindow(int i, int order) {
  return 0.54 - 0.46 * std::cos(2.0 * M_PI * i / (order - 1));
}

} // namespace

IModule *createModule() { return new BandPassComputeComplex(); }

BandPassComputeComplex::BandPassComputeComplex()
    : IModule({"BandPassComputeComplex", "libBandPassComputeComplex-module.so",
               "module.json"}) {}

BandPassComputeComplex::~BandPassComputeComplex() = default;

bool BandPassComputeComplex::init() {
  if (m_sampleRate <= 0.0) {
    ERROR << "BandPassComputeComplex::init failed: sample rate must be > 0."
          << std::endl;
    return false;
  }

  if (m_filterOrder <= 0 || (m_filterOrder % 2) == 0) {
    ERROR << "BandPassComputeComplex::init failed: filter order must be "
             "positive odd number."
          << std::endl;
    return false;
  }

  const double nyquist = m_sampleRate * 0.5;
  const int mid = (m_filterOrder - 1) / 2;
  if (m_sidebandExplicit && m_sideband == Sideband::Both) {
    if (m_lowCutoff < 0.0 || m_highCutoff <= m_lowCutoff) {
      ERROR << "BandPassComputeComplex::init failed: cutoff range is invalid "
               "for sideband='both'."
            << std::endl;
      return false;
    }
    if (m_highCutoff >= nyquist) {
      ERROR << "BandPassComputeComplex::init failed: high cutoff must be less "
               "than Nyquist frequency."
            << std::endl;
      return false;
    }

    auto coeff = std::make_unique<float[]>(m_filterOrder);

    const double f1 = m_lowCutoff / m_sampleRate;
    const double f2 = m_highCutoff / m_sampleRate;

    for (int i = 0; i < m_filterOrder; ++i) {
      const int k = i - mid;

      double val;
      if (k == 0) {
        val = 2.0 * (f2 - f1);
      } else {
        val = (std::sin(2.0 * M_PI * f2 * k) - std::sin(2.0 * M_PI * f1 * k)) /
              (M_PI * k);
      }

      const double w = hammingWindow(i, m_filterOrder);
      coeff[i] = static_cast<float>(val * w);
    }

    m_data = std::make_shared<CpuFloatSignal>(coeff.release(), m_filterOrder);
    m_isComputed = false;
    return m_data && m_data->isValid();
  }

  double bandLow = 0.0;
  double bandHigh = 0.0;
  if (m_sidebandExplicit) {
    if (m_lowCutoff < 0.0 || m_highCutoff <= m_lowCutoff) {
      ERROR << "BandPassComputeComplex::init failed: cutoff range is invalid "
               "for explicit sideband mode."
            << std::endl;
      return false;
    }
    if (m_highCutoff >= nyquist) {
      ERROR << "BandPassComputeComplex::init failed: high cutoff must be less "
               "than Nyquist frequency."
            << std::endl;
      return false;
    }

    if (m_sideband == Sideband::Positive) {
      bandLow = m_lowCutoff;
      bandHigh = m_highCutoff;
    } else {
      bandLow = -m_highCutoff;
      bandHigh = -m_lowCutoff;
    }
  } else {
    if (m_highCutoff <= m_lowCutoff) {
      ERROR << "BandPassComputeComplex::init failed: signed cutoff range is "
               "invalid."
            << std::endl;
      return false;
    }
    if (m_lowCutoff <= -nyquist || m_highCutoff >= nyquist) {
      ERROR << "BandPassComputeComplex::init failed: signed cutoffs must be "
               "inside (-Nyquist, Nyquist)."
            << std::endl;
      return false;
    }

    bandLow = m_lowCutoff;
    bandHigh = m_highCutoff;
  }

  auto coeff = std::make_unique<cuComplex[]>(m_filterOrder);

  const double halfBw = (bandHigh - bandLow) * 0.5;
  const double fc = (bandHigh + bandLow) * 0.5;

  for (int i = 0; i < m_filterOrder; ++i) {
    const int k = i - mid;
    const double n = static_cast<double>(k);

    const double lpNorm = 2.0 * halfBw / m_sampleRate;
    const double lp = lpNorm * sinc(2.0 * halfBw * n / m_sampleRate);
    const double w = hammingWindow(i, m_filterOrder);
    const double env = lp * w;

    const double phase = 2.0 * M_PI * fc * n / m_sampleRate;
    coeff[i] = make_cuComplex(static_cast<float>(env * std::cos(phase)),
                              static_cast<float>(env * std::sin(phase)));
  }

  m_data = std::make_shared<CpuComplexSignal>(coeff.release(), m_filterOrder);
  m_isComputed = false;
  return m_data && m_data->isValid();
}

bool BandPassComputeComplex::run() {
  if (!m_data) {
    ERROR << "BandPassComputeComplex::run failed: coefficients were not "
             "initialized."
          << std::endl;
    return false;
  }

  DEBUG << "BandPassComputeComplex::run data size: " << m_data->size()
        << std::endl;
  return true;
}

void BandPassComputeComplex::setParam(const std::string &paramName,
                                      const std::any &value) {
  if (paramName == "sample rate") {
    m_sampleRate = static_cast<double>(std::any_cast<int64_t>(value));
    DEBUG << "BandPassComputeComplex sample rate set to: " << m_sampleRate
          << std::endl;
    return;
  }

  if (paramName == "filter order") {
    m_filterOrder = std::any_cast<int64_t>(value);
    DEBUG << "BandPassComputeComplex filter order set to: " << m_filterOrder
          << std::endl;
    return;
  }

  if (paramName == "block size") {
    (void)std::any_cast<int64_t>(value);
    DEBUG << "BandPassComputeComplex::setParam: 'block size' is accepted for "
             "compatibility and ignored."
          << std::endl;
    return;
  }

  if (paramName == "low cutoff") {
    m_lowCutoff = std::any_cast<double>(value);
    DEBUG << "BandPassComputeComplex low cutoff set to: " << m_lowCutoff
          << std::endl;
    return;
  }

  if (paramName == "high cutoff") {
    m_highCutoff = std::any_cast<double>(value);
    DEBUG << "BandPassComputeComplex high cutoff set to: " << m_highCutoff
          << std::endl;
    return;
  }

  if (paramName == "sideband") {
    const auto sideband = std::any_cast<std::string>(value);
    if (!parseSideband(sideband)) {
      ERROR << "BandPassComputeComplex::setParam unknown sideband: " << sideband
            << std::endl;
    }
    m_sidebandExplicit = true;
    return;
  }

  ERROR << "BandPassComputeComplex::setParam unknown parameter: " << paramName
        << std::endl;
}

bool BandPassComputeComplex::setData(std::shared_ptr<IData> data) {
  (void)data;
  return true;
}

std::shared_ptr<IData> BandPassComputeComplex::getData() {
  if (m_data && !m_isComputed) {
    m_isComputed = true;
    return m_data;
  }
  return std::make_shared<EmptyContainer>();
}

bool BandPassComputeComplex::parseSideband(const std::string &value) {
  if (value == "negative") {
    m_sideband = Sideband::Negative;
    return true;
  }
  if (value == "positive") {
    m_sideband = Sideband::Positive;
    return true;
  }
  if (value == "both") {
    m_sideband = Sideband::Both;
    return true;
  }
  return false;
}
