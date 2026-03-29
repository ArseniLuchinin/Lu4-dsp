#include <CarrierPhaseEstimator.hpp>
#include <VariablesResolve.hpp>

#include <CpuFloatSignal.hpp>
#include <VirtualTransmitter.hpp>
#include <module.hpp>

#include <cmath>
#include <complex>
#include <memory>

IModule* createModule() {
    return new CarrierPhaseEstimator();
}

CarrierPhaseEstimator::CarrierPhaseEstimator()
    : IModule({"CarrierPhaseEstimator", "", ""})
{}

CarrierPhaseEstimator::~CarrierPhaseEstimator() = default;

bool CarrierPhaseEstimator::init() {
    if (m_phaseTag.empty()) {
        ERROR << "CarrierPhaseEstimator::init failed: phase tag is empty." << std::endl;
        return false;
    }
    return true;
}

bool CarrierPhaseEstimator::setData(std::shared_ptr<IData> data) {
    m_inData = std::dynamic_pointer_cast<CpuComplexSignal>(data);
    if (!m_inData) {
        ERROR << "CarrierPhaseEstimator::setData failed: input must be CpuComplexSignal." << std::endl;
        return false;
    }
    if (!m_inData->isValid()) {
        ERROR << "CarrierPhaseEstimator::setData failed: input signal is invalid." << std::endl;
        return false;
    }

    m_outData = m_inData;
    return true;
}

bool CarrierPhaseEstimator::run() {
    if (!m_inData) {
        ERROR << "CarrierPhaseEstimator::run failed: input data is null." << std::endl;
        return false;
    }

    const size_t n = m_inData->size();
    std::complex<double> sum(0.0, 0.0);

    const cuComplex* data = m_inData->getData();
    for (size_t i = 0; i < n; ++i) {
        const std::complex<double> x(static_cast<double>(data[i].x), static_cast<double>(data[i].y));
        const std::complex<double> x4 = x * x * x * x;
        sum += x4;
    }

    if (n == 0 || (sum.real() == 0.0 && sum.imag() == 0.0)) {
        m_estimatedPhase = 0.0f;
    } else {
        constexpr float quarterPi = static_cast<float>(M_PI_4);
        constexpr float halfPi = static_cast<float>(M_PI_2);

        float phase = static_cast<float>(std::atan2(sum.imag(), sum.real()) * 0.25) - quarterPi;
        while (phase >= quarterPi) {
            phase -= halfPi;
        }
        while (phase < -quarterPi) {
            phase += halfPi;
        }
        m_estimatedPhase = phase;
    }

    auto* phaseRaw = new float[1];
    phaseRaw[0] = m_estimatedPhase;
    auto phaseData = std::make_shared<CpuFloatSignal>(phaseRaw, 1);

    VirtualTransmitter transmitter;
    transmitter.txData(phaseData, m_phaseTag);
    return true;
}

void CarrierPhaseEstimator::setParam(const std::string& paramName, const std::any& value) {
    const std::any resolved = resolveParamValue(value);

    if (paramName == "phase tag") {
        m_phaseTag = std::any_cast<std::string>(resolved);
        return;
    }

    ERROR << "CarrierPhaseEstimator::setParam unknown parameter: " << paramName << std::endl;
}

std::shared_ptr<IData> CarrierPhaseEstimator::getData() {
    return m_outData;
}
