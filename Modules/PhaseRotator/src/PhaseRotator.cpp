#include <PhaseRotator.hpp>
#include <VariablesResolve.hpp>

#include <GpuFloatSignal.hpp>
#include <module.hpp>

#include <cuda_runtime.h>

#include <algorithm>
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
    m_inData = std::dynamic_pointer_cast<GpuComplexFloatSignal>(data);
    if (!m_inData) {
        ERROR << "PhaseRotator::setData failed: input must be GpuComplexFloatSignal." << std::endl;
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

    const auto phaseData = std::dynamic_pointer_cast<GpuFloatSignal>(rxData());
    if (!phaseData || phaseData->size() == 0 || !phaseData->getDeviceData()) {
        ERROR << "PhaseRotator::run failed: phase data is missing or invalid." << std::endl;
        return false;
    }

    float phase = 0.0f;
    const auto phaseCopyErr = cudaMemcpy(
        &phase,
        phaseData->getDeviceData(),
        sizeof(float),
        cudaMemcpyDeviceToHost
    );
    if (phaseCopyErr != cudaSuccess) {
        ERROR << "PhaseRotator::run failed: phase cudaMemcpy D2H failed: "
              << cudaGetErrorString(phaseCopyErr) << std::endl;
        return false;
    }

    const float c = std::cos(phase);
    const float s = std::sin(phase);

    const size_t n = m_inData->size();
    auto out = std::make_shared<GpuComplexFloatSignal>(std::max<size_t>(size_t(1), n));
    if (!out || !out->isValid()) {
        ERROR << "PhaseRotator::run failed: output allocation failed." << std::endl;
        return false;
    }
    if (!out->setLogicalSize(n)) {
        ERROR << "PhaseRotator::run failed: unable to set output logical size." << std::endl;
        return false;
    }

    extern cudaError_t launchPhaseRotatorKernel(
        const cuComplex* in,
        cuComplex* out,
        size_t n,
        float c,
        float s,
        int blocks,
        int threads
    );

    if (n > 0) {
        const int threads = 256;
        const int blocks = static_cast<int>((n + static_cast<size_t>(threads) - 1) / static_cast<size_t>(threads));

        const auto launchErr = launchPhaseRotatorKernel(
            m_inData->getDeviceData(),
            out->getDeviceData(),
            n,
            c,
            s,
            blocks,
            threads
        );
        if (launchErr != cudaSuccess) {
            ERROR << "PhaseRotator::run failed: kernel launch failed: " << cudaGetErrorString(launchErr) << std::endl;
            return false;
        }

        const auto syncErr = cudaDeviceSynchronize();
        if (syncErr != cudaSuccess) {
            ERROR << "PhaseRotator::run failed: kernel execution failed: " << cudaGetErrorString(syncErr) << std::endl;
            return false;
        }
    }

    m_outData = out;
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
