#include <RRCCompute.hpp>
#include <VariablesResolve.hpp>

#include <EmptyContainer.hpp>
#include <module.hpp>

#include <any>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>

namespace {

bool tryAnyToDouble(const std::any &value, double *out) {
  if (!out) {
    return false;
  }

  if (value.type() == typeid(double)) {
    *out = std::any_cast<double>(value);
    return true;
  }
  if (value.type() == typeid(float)) {
    *out = static_cast<double>(std::any_cast<float>(value));
    return true;
  }
  if (value.type() == typeid(int64_t)) {
    *out = static_cast<double>(std::any_cast<int64_t>(value));
    return true;
  }
  if (value.type() == typeid(uint64_t)) {
    *out = static_cast<double>(std::any_cast<uint64_t>(value));
    return true;
  }
  return false;
}

bool tryAnyToInt(const std::any &value, int *out) {
  if (!out) {
    return false;
  }

  if (value.type() == typeid(int64_t)) {
    *out = static_cast<int>(std::any_cast<int64_t>(value));
    return true;
  }
  if (value.type() == typeid(uint64_t)) {
    *out = static_cast<int>(std::any_cast<uint64_t>(value));
    return true;
  }
  return false;
}

double rrcTapValue(double x, double rolloff) {
  constexpr double pi = M_PI;
  constexpr double eps = 1.0e-12;

  if (std::abs(x) < eps) {
    return 1.0 + rolloff * ((4.0 / pi) - 1.0);
  }

  if (rolloff > 0.0) {
    const double singularity = std::abs(4.0 * rolloff * x);
    if (std::abs(singularity - 1.0) < 1.0e-8) {
      const double a = pi / (4.0 * rolloff);
      return (rolloff / std::sqrt(2.0)) * (((1.0 + (2.0 / pi)) * std::sin(a)) +
                                           ((1.0 - (2.0 / pi)) * std::cos(a)));
    }
  }

  const double numerator =
      std::sin(pi * x * (1.0 - rolloff)) +
      (4.0 * rolloff * x * std::cos(pi * x * (1.0 + rolloff)));
  const double denominator = pi * x * (1.0 - std::pow(4.0 * rolloff * x, 2.0));

  if (std::abs(denominator) < eps) {
    return 0.0;
  }

  return numerator / denominator;
}

} // namespace

IModule *createModule() { return new RRCCompute(); }

RRCCompute::RRCCompute()
    : IModule({"RRCCompute", "libRRCCompute-module.so", "module.json"}) {}

RRCCompute::~RRCCompute() = default;

bool RRCCompute::init() {
  if (m_sampleRate <= 0.0) {
    ERROR << "RRCCompute::init failed: sample rate must be > 0." << std::endl;
    return false;
  }

  if (m_symbolRate <= 0.0) {
    ERROR << "RRCCompute::init failed: symbol rate must be > 0." << std::endl;
    return false;
  }

  if (m_rolloff < 0.0 || m_rolloff > 1.0) {
    ERROR << "RRCCompute::init failed: rolloff must be inside [0, 1]."
          << std::endl;
    return false;
  }

  if (m_spanSymbols <= 0) {
    ERROR << "RRCCompute::init failed: span symbols must be > 0." << std::endl;
    return false;
  }

  if (!m_samplesPerSymbolExplicit) {
    const double ratio = m_sampleRate / m_symbolRate;
    const double rounded = std::round(ratio);
    if (std::abs(ratio - rounded) > 1.0e-9) {
      ERROR << "RRCCompute::init failed: sample rate must be divisible by "
               "symbol rate when samples per symbol is not set."
            << std::endl;
      return false;
    }
    m_samplesPerSymbol = static_cast<int>(rounded);
  }

  if (m_samplesPerSymbol <= 1) {
    ERROR << "RRCCompute::init failed: samples per symbol must be > 1."
          << std::endl;
    return false;
  }

  const int tapsCount = (m_spanSymbols * m_samplesPerSymbol) + 1;
  if (tapsCount <= 1 || (tapsCount % 2) == 0) {
    ERROR << "RRCCompute::init failed: taps count must be positive odd number."
          << std::endl;
    return false;
  }

  auto taps = std::make_unique<float[]>(static_cast<size_t>(tapsCount));
  const int center = (tapsCount - 1) / 2;

  for (int i = 0; i < tapsCount; ++i) {
    const int n = i - center;
    const double x =
        static_cast<double>(n) / static_cast<double>(m_samplesPerSymbol);
    const double value = rrcTapValue(x, m_rolloff);
    if (!std::isfinite(value)) {
      ERROR << "RRCCompute::init failed: non-finite tap value generated."
            << std::endl;
      return false;
    }
    taps[static_cast<size_t>(i)] = static_cast<float>(value);
  }

  if (m_normalizeGain) {
    double sum = 0.0;
    for (int i = 0; i < tapsCount; ++i) {
      sum += static_cast<double>(taps[static_cast<size_t>(i)]);
    }

    if (std::abs(sum) <= std::numeric_limits<double>::epsilon()) {
      ERROR << "RRCCompute::init failed: taps sum is zero, normalization is "
               "impossible."
            << std::endl;
      return false;
    }

    const float inv = static_cast<float>(1.0 / sum);
    for (int i = 0; i < tapsCount; ++i) {
      taps[static_cast<size_t>(i)] *= inv;
    }
  }

  m_data = std::make_shared<GpuFloatSignal>(static_cast<size_t>(tapsCount));
  if (!m_data || !m_data->isValid()) {
    ERROR << "RRCCompute::init failed: unable to allocate GPU taps buffer."
          << std::endl;
    return false;
  }
  m_data->setDataFromHost(taps.get(), static_cast<size_t>(tapsCount));
  if (!m_data->isValid()) {
    ERROR << "RRCCompute::init failed: unable to upload taps to GPU."
          << std::endl;
    return false;
  }
  m_isComputed = false;
  return m_data && m_data->isValid();
}

bool RRCCompute::run() {
  if (!m_data) {
    ERROR << "RRCCompute::run failed: coefficients were not initialized."
          << std::endl;
    return false;
  }

  DEBUG << "RRCCompute::run taps count: " << m_data->size() << std::endl;
  return true;
}

void RRCCompute::setParam(const std::string &paramName, const std::any &value) {
  if (paramName == "sample rate") {
    double parsed = 0.0;
    if (!tryAnyToDouble(value, &parsed)) {
      ERROR << "RRCCompute::setParam failed: invalid sample rate type."
            << std::endl;
      return;
    }
    m_sampleRate = parsed;
    return;
  }

  if (paramName == "symbol rate") {
    double parsed = 0.0;
    if (!tryAnyToDouble(value, &parsed)) {
      ERROR << "RRCCompute::setParam failed: invalid symbol rate type."
            << std::endl;
      return;
    }
    m_symbolRate = parsed;
    return;
  }

  if (paramName == "rolloff") {
    double parsed = 0.0;
    if (!tryAnyToDouble(value, &parsed)) {
      ERROR << "RRCCompute::setParam failed: invalid rolloff type."
            << std::endl;
      return;
    }
    m_rolloff = parsed;
    return;
  }

  if (paramName == "span symbols") {
    int parsed = 0;
    if (!tryAnyToInt(value, &parsed)) {
      ERROR << "RRCCompute::setParam failed: invalid span symbols type."
            << std::endl;
      return;
    }
    m_spanSymbols = parsed;
    return;
  }

  if (paramName == "samples per symbol") {
    int parsed = 0;
    if (!tryAnyToInt(value, &parsed)) {
      ERROR << "RRCCompute::setParam failed: invalid samples per symbol type."
            << std::endl;
      return;
    }
    m_samplesPerSymbol = parsed;
    m_samplesPerSymbolExplicit = true;
    return;
  }

  if (paramName == "normalize gain") {
    m_normalizeGain = std::any_cast<bool>(value);
    return;
  }

  if (paramName == "block size") {
    (void)value;
    DEBUG << "RRCCompute::setParam: 'block size' is accepted for compatibility "
             "and ignored."
          << std::endl;
    return;
  }

  ERROR << "RRCCompute::setParam unknown parameter: " << paramName << std::endl;
}

bool RRCCompute::setData(std::shared_ptr<IData> data) {
  (void)data;
  return true;
}

std::shared_ptr<IData> RRCCompute::getData() {
  if (m_data && !m_isComputed) {
    m_isComputed = true;
    return m_data;
  }

  return std::make_shared<EmptyContainer>();
}
