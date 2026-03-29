#include <PhaseRotator.hpp>
#include <VariablesResolve.hpp>

#include <CpuFloatSignal.hpp>
#include <module.hpp>

#include <cmath>

IModule* createModule() {
    return new PhaseRotator();
}

PhaseRotator::PhaseRotator()
    : IModule({"PhaseRotator", "", ""})
    , IVirtualRX()
{}

PhaseRotator::~PhaseRotator() = default;

bool PhaseRotator::init() {
    if (m_phaseTag.empty()) {
        ERROR << "PhaseRotator::init failed: phase tag is empty." << std::endl;
        return false;
    }

    if (!setTag(m_phaseTag)) {
        ERROR << "PhaseRotator::init failed: unable to set phase tag." << std::endl;
        return false;
    }
    return true;
}

bool PhaseRotator::setData(std::shared_ptr<IData> data) {
    m_inData = std::dynamic_pointer_cast<CpuComplexSignal>(data);
    if (!m_inData) {
        ERROR << "PhaseRotator::setData failed: input must be CpuComplexSignal." << std::endl;
        return false;
    }

    if (!m_inData->isValid()) {
        ERROR << "PhaseRotator::setData failed: input signal is invalid." << std::endl;
        return false;
    }

    return true;
}

bool PhaseRotator::run() {
    if (!m_inData) {
        ERROR << "PhaseRotator::run failed: input data is null." << std::endl;
        return false;
    }

    const auto phaseData = std::dynamic_pointer_cast<CpuFloatSignal>(rxData());
    if (!phaseData || phaseData->size() == 0 || !phaseData->getData()) {
        ERROR << "PhaseRotator::run failed: phase data is missing or invalid." << std::endl;
        return false;
    }

    const float phase = phaseData->getData()[0];
    const float c = std::cos(phase);
    const float s = std::sin(phase);

    const size_t n = m_inData->size();
    auto* rotated = new cuComplex[n];

    const cuComplex* in = m_inData->getData();
    for (size_t i = 0; i < n; ++i) {
        const float re = in[i].x;
        const float im = in[i].y;

        rotated[i].x = (re * c) + (im * s);
        rotated[i].y = (-re * s) + (im * c);
    }

    m_outData = std::make_shared<CpuComplexSignal>(rotated, n);
    return true;
}

void PhaseRotator::setParam(const std::string& paramName, const std::any& value) {
    const std::any resolved = resolveParamValue(value);

    if (paramName == "phase tag") {
        m_phaseTag = std::any_cast<std::string>(resolved);
        return;
    }

    ERROR << "PhaseRotator::setParam unknown parameter: " << paramName << std::endl;
}

std::shared_ptr<IData> PhaseRotator::getData() {
    return m_outData;
}
