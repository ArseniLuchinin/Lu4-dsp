#include <BandPassCompute.hpp>
#include <VariablesResolve.hpp>

#include <module.hpp>
#include <CpuFloatSignal.hpp>
#include <EmptyContainer.hpp>

#include <cmath>
#include <memory>

IModule* createModule() {
    return new BandPassCompute();
}

BandPassCompute::BandPassCompute()
    : IModule({"BandPassCompute", "", ""})
    , m_sampleRate(0.0f)
    , m_filterOrder(0)
    , m_blockSize(0)
    , m_lowCutoff(0.0f)
    , m_highCutoff(0.0f)
{}

BandPassCompute::~BandPassCompute() = default;

bool BandPassCompute::init() {
    if (m_sampleRate <= 0.0f) {
        ERROR << "BandPassCompute::init failed: sample rate must be > 0." << std::endl;
        return false;
    }

    if (m_filterOrder <= 0 || (m_filterOrder % 2) == 0) {
        ERROR << "BandPassCompute::init failed: filter order must be positive odd number." << std::endl;
        return false;
    }

    if (m_lowCutoff < 0.0f || m_highCutoff <= m_lowCutoff) {
        ERROR << "BandPassCompute::init failed: cutoff range is invalid." << std::endl;
        return false;
    }

    const float nyquist = m_sampleRate * 0.5f;
    if (m_highCutoff >= nyquist) {
        ERROR << "BandPassCompute::init failed: high cutoff must be less than Nyquist frequency." << std::endl;
        return false;
    }

    auto coeff = std::make_unique<float[]>(m_filterOrder);

    float f1 = m_lowCutoff / m_sampleRate;
    float f2 = m_highCutoff / m_sampleRate;

    int mid = (m_filterOrder - 1) / 2;

    for (int i = 0; i < m_filterOrder; ++i)
    {
        int k = i - mid;

        float val;

        if (k == 0)
            val = 2.0f * (f2 - f1);
        else
            val = (sinf(2.0f * M_PI * f2 * k)
                 - sinf(2.0f * M_PI * f1 * k))
                 / (M_PI * k);

        float w = 0.54f - 0.46f *
                  cosf(2.0f * M_PI * i / (m_filterOrder - 1));

        coeff[i] = val * w;
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

    INFO << "BandPassCompute run with data size: " << m_data->size() << std::endl;
    return true;
}

void BandPassCompute::setParam(const std::string& paramName, const std::any& value) {
    const std::any resolved = resolveParamValue(value);
    if (paramName == "sample rate") {
        m_sampleRate = static_cast<float>(std::any_cast<int32_t>(resolved));
        INFO << "BandPassCompute sample rate set to: " << m_sampleRate << std::endl;
        return;
    }

    if (paramName == "filter order") {
        m_filterOrder = std::any_cast<int32_t>(resolved);
        INFO << "BandPassCompute filter order set to: " << m_filterOrder << std::endl;
        return;
    }

    if (paramName == "block size") {
        m_blockSize = std::any_cast<int32_t>(resolved);
        INFO << "BandPassCompute block size set to: " << m_blockSize << std::endl;
        return;
    }

    if (paramName == "low cutoff") {
        m_lowCutoff = std::any_cast<float>(resolved);
        INFO << "BandPassCompute low cutoff set to: " << m_lowCutoff << std::endl;
        return;
    }

    if (paramName == "high cutoff") {
        m_highCutoff = std::any_cast<float>(resolved);
        INFO << "BandPassCompute high cutoff set to: " << m_highCutoff << std::endl;
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
