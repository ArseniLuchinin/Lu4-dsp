#include <QPSKDecision.hpp>

#include <module.hpp>

#include <cuda_runtime.h>

#include <algorithm>

IModule* createModule() {
    return new QPSKDecision();
}

QPSKDecision::QPSKDecision()
    : IModule({"QPSKDecision", "", ""})
{}

QPSKDecision::~QPSKDecision() = default;

bool QPSKDecision::init() {
    return true;
}

bool QPSKDecision::setData(std::shared_ptr<IData> data) {
    m_inData = std::dynamic_pointer_cast<GpuComplexFloatSignal>(data);
    if (!m_inData) {
        ERROR << "QPSKDecision::setData failed: input must be GpuComplexFloatSignal." << std::endl;
        return false;
    }

    if (!m_inData->isValid()) {
        ERROR << "QPSKDecision::setData failed: input signal is invalid." << std::endl;
        return false;
    }

    return true;
}

bool QPSKDecision::run() {
    if (!m_inData) {
        ERROR << "QPSKDecision::run failed: input data is null." << std::endl;
        return false;
    }

    const size_t n = m_inData->size();
    const size_t bitCount = n * 2;
    auto out = std::make_shared<GpuByteSignal>(std::max<size_t>(size_t(1), bitCount));
    if (!out || !out->isValid()) {
        ERROR << "QPSKDecision::run failed: output allocation failed." << std::endl;
        return false;
    }
    if (!out->setLogicalSize(bitCount)) {
        ERROR << "QPSKDecision::run failed: unable to set output logical size." << std::endl;
        return false;
    }

    extern cudaError_t launchQpskDecisionKernel(
        const cuComplex* in,
        uint8_t* outBits,
        size_t symbolsCount,
        int blocks,
        int threads
    );

    if (n > 0) {
        const int threads = 256;
        const int blocks = static_cast<int>((n + static_cast<size_t>(threads) - 1) / static_cast<size_t>(threads));
        const auto launchErr = launchQpskDecisionKernel(
            m_inData->getDeviceData(),
            out->getDeviceData(),
            n,
            blocks,
            threads
        );
        if (launchErr != cudaSuccess) {
            ERROR << "QPSKDecision::run failed: kernel launch failed: " << cudaGetErrorString(launchErr) << std::endl;
            return false;
        }

        // TODO Нужна ли здесь синхронизация?
        const auto syncErr = cudaDeviceSynchronize();
        if (syncErr != cudaSuccess) {
            ERROR << "QPSKDecision::run failed: kernel execution failed: " << cudaGetErrorString(syncErr) << std::endl;
            return false;
        }
    }

    m_outData = out;
    return true;
}

void QPSKDecision::setParam(const std::string& paramName, const std::any& value) {
    (void)value;
    ERROR << "QPSKDecision::setParam unknown parameter: " << paramName << std::endl;
}

std::shared_ptr<IData> QPSKDecision::getData() {
    return m_outData;
}
