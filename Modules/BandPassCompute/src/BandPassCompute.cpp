#include <BandPassCompute.hpp>
#include <VariablesResolve.hpp>

#include <module.hpp>
#include <CpuFloatSignal.hpp>
#include <EmptyContainer.hpp>

#include <cmath>
#include <memory>

namespace {

double hammingWindow(int i, int order)
{
    return 0.54 - 0.46 * std::cos(2.0 * M_PI * i / (order - 1));
}

bool isBasicCutoffRangeValid(double lowCutoff, double highCutoff)
{
    return lowCutoff >= 0.0 && highCutoff > lowCutoff;
}

bool isNyquistBoundValid(double sampleRate, double highCutoff)
{
    const double nyquist = sampleRate * 0.5;
    return highCutoff < nyquist;
}

} // namespace

IModule* createModule() {
    return new BandPassCompute();
}

BandPassCompute::BandPassCompute()
    : IModule({"BandPassCompute", "", ""})
    , m_sampleRate(0.0)
    , m_filterOrder(0)
    , m_lowCutoff(0.0)
    , m_highCutoff(0.0)
{}

BandPassCompute::~BandPassCompute() = default;

bool BandPassCompute::init() {
    if (m_sampleRate <= 0.0) {
        ERROR << "BandPassCompute::init failed: sample rate must be > 0." << std::endl;
        return false;
    }

    if (m_filterOrder <= 0 || (m_filterOrder % 2) == 0) {
        ERROR << "BandPassCompute::init failed: filter order must be positive odd number." << std::endl;
        return false;
    }

    if (!isBasicCutoffRangeValid(m_lowCutoff, m_highCutoff)) {
        ERROR << "BandPassCompute::init failed: cutoff range is invalid." << std::endl;
        return false;
    }
    if (!isNyquistBoundValid(m_sampleRate, m_highCutoff)) {
        ERROR << "BandPassCompute::init failed: high cutoff must be less than Nyquist frequency." << std::endl;
        return false;
    }

    auto coeff = std::make_unique<float[]>(m_filterOrder);

    const double f1 = m_lowCutoff / m_sampleRate;
    const double f2 = m_highCutoff / m_sampleRate;

    int mid = (m_filterOrder - 1) / 2;

    for (int i = 0; i < m_filterOrder; ++i)
    {
        int k = i - mid;

        double val;

        if (k == 0)
            val = 2.0 * (f2 - f1);
        else
            val = (sin(2.0 * M_PI * f2 * k)
                 - sin(2.0 * M_PI * f1 * k))
                 / (M_PI * k);

        const double w = hammingWindow(i, m_filterOrder);

        coeff[i] = static_cast<float>(val * w);
    }

    m_data = std::make_shared<CpuFloatSignal>(coeff.release(), m_filterOrder);
    m_isComputed = false;
    return m_data && m_data->isValid();
}

bool BandPassCompute::run() {
    if (!m_data) {
        ERROR << "BandPassCompute::run failed: coefficients were not initialized." << std::endl;
        return false;
    }

    DEBUG << "BandPassCompute::run data size: " << m_data->size() << std::endl;
    return true;
}

void BandPassCompute::setParam(const std::string& paramName, const std::any& value) {
    if (paramName == "sample rate") {
        m_sampleRate = static_cast<double>(std::any_cast<int32_t>(value));
        DEBUG << "BandPassCompute sample rate set to: " << m_sampleRate << std::endl;
        return;
    }

    if (paramName == "filter order") {
        m_filterOrder = std::any_cast<int32_t>(value);
        DEBUG << "BandPassCompute filter order set to: " << m_filterOrder << std::endl;
        return;
    }

    if (paramName == "block size") {
        (void)std::any_cast<int32_t>(value);
        DEBUG << "BandPassCompute::setParam: 'block size' is accepted for compatibility and ignored." << std::endl;
        return;
    }

    if (paramName == "low cutoff") {
        m_lowCutoff = std::any_cast<double>(value);
        DEBUG << "BandPassCompute low cutoff set to: " << m_lowCutoff << std::endl;
        return;
    }

    if (paramName == "high cutoff") {
        m_highCutoff = std::any_cast<double>(value);
        DEBUG << "BandPassCompute high cutoff set to: " << m_highCutoff << std::endl;
        return;
    }

    ERROR << "BandPassCompute::setParam unknown parameter: " << paramName << std::endl;
}

bool BandPassCompute::setData(std::shared_ptr<IData> data) {
    (void)data;
    return true;
}

std::shared_ptr<IData> BandPassCompute::getData() {
    if (m_data && !m_isComputed) {
        m_isComputed = true;
        return m_data;
    }

    return std::make_shared<EmptyContainer>();
}
