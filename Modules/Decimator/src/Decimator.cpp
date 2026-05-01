#include <Decimator.hpp>
#include <VariablesResolve.hpp>

#include <module.hpp>

#include <cuda_runtime.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

cudaError_t launchDecimatorKernel(
    const cuComplex* in,
    cuComplex* out,
    size_t outCount,
    size_t phase,
    size_t sps,
    int blocks,
    int threads
);

namespace {

bool anyToInt(const std::any& value, int* out) {
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

} // namespace

namespace {

bool runDecimatorCuda(
    const cuComplex* in,
    size_t inputSize,
    cuComplex* out,
    int samplesPerSymbol,
    size_t currentPhase,
    std::string* error)
{
    if (inputSize == 0) {
        return true;
    }

    const size_t sps = static_cast<size_t>(samplesPerSymbol);
    if (currentPhase >= inputSize) {
        return true;
    }

    const size_t outCount = 1 + ((inputSize - 1 - currentPhase) / sps);
    if (outCount == 0) {
        return true;
    }

    const int threads = 256;
    const int blocks = static_cast<int>((outCount + static_cast<size_t>(threads) - 1) / static_cast<size_t>(threads));
    const auto launchErr = launchDecimatorKernel(in, out, outCount, currentPhase, sps, blocks, threads);
    if (launchErr != cudaSuccess) {
        if (error) {
            *error = std::string("Decimator::run failed: kernel launch failed: ") + cudaGetErrorString(launchErr);
        }
        return false;
    }

    return true;
}

} // namespace

IModule* createModule() {
    return new Decimator();
}

Decimator::Decimator()
    : IModule({"Decimator", "libDecimator-module.so", "module.json"})
{}

Decimator::~Decimator() = default;

bool Decimator::init() {
    if (m_samplesPerSymbol <= 0) {
        ERROR << "Decimator::init failed: samples per symbol must be > 0." << std::endl;
        return false;
    }

    if (m_offset < 0 || m_offset >= m_samplesPerSymbol) {
        ERROR << "Decimator::init failed: offset must be in [0, samples per symbol)." << std::endl;
        return false;
    }

    m_currentPhase = static_cast<size_t>(m_offset);
    m_initializedState = true;
    // Warm-up kernel/module path to avoid first-run latency spike on the first real block.
    const auto tWarmupStart = std::chrono::steady_clock::now();
    auto warmupIn = std::make_shared<GpuComplexFloatSignal>(1);
    auto warmupOut = std::make_shared<GpuComplexFloatSignal>(1);
    if (!warmupIn || !warmupOut || !warmupIn->isValid() || !warmupOut->isValid()) {
        ERROR << "Decimator::init failed: unable to allocate warm-up buffers." << std::endl;
        return false;
    }

    cuComplex warmSample = make_cuComplex(0.0f, 0.0f);
    warmupIn->setDataFromHost(&warmSample, 1);
    if (!warmupIn->isValid()) {
        ERROR << "Decimator::init failed: unable to upload warm-up sample." << std::endl;
        return false;
    }

    std::string warmupError;
    if (!runDecimatorCuda(
            warmupIn->getDeviceData(),
            1,
            warmupOut->getDeviceData(),
            m_samplesPerSymbol,
            0,
            &warmupError)) {
        ERROR << "Decimator::init warm-up failed: " << warmupError << std::endl;
        return false;
    }

    const auto warmupSyncErr = cudaDeviceSynchronize();
    if (warmupSyncErr != cudaSuccess) {
        ERROR << "Decimator::init warm-up sync failed: " << cudaGetErrorString(warmupSyncErr) << std::endl;
        return false;
    }

    const auto tWarmupEnd = std::chrono::steady_clock::now();
    const auto warmupTotalUs = std::chrono::duration_cast<std::chrono::microseconds>(tWarmupEnd - tWarmupStart).count();
    DEBUG << "Decimator::init warm-up elapsed_total_us=" << warmupTotalUs << std::endl;
    return true;
}

bool Decimator::setData(std::shared_ptr<IData> data) {
    m_inData = std::dynamic_pointer_cast<GpuComplexFloatSignal>(data);
    if (!m_inData) {
        ERROR << "Decimator::setData failed: input must be GpuComplexFloatSignal." << std::endl;
        return false;
    }

    if (!m_inData->isValid()) {
        ERROR << "Decimator::setData failed: input signal is invalid." << std::endl;
        return false;
    }

    return true;
}

bool Decimator::run() {
    if (!m_initializedState) {
        ERROR << "Decimator::run failed: module is not initialized." << std::endl;
        return false;
    }

    if (!m_inData) {
        ERROR << "Decimator::run failed: input data is null." << std::endl;
        return false;
    }

    const size_t inputSize = m_inData->size();
    if (inputSize == 0) {
        auto empty = std::make_shared<GpuComplexFloatSignal>(1);
        if (!empty || !empty->isValid() || !empty->setLogicalSize(0)) {
            ERROR << "Decimator::run failed: unable to allocate empty output." << std::endl;
            return false;
        }
        m_outData = empty;
        return true;
    }

    const size_t sps = static_cast<size_t>(m_samplesPerSymbol);
    const size_t outCount = (m_currentPhase < inputSize) ? (1 + ((inputSize - 1 - m_currentPhase) / sps)) : 0;
    const size_t requiredCapacity = std::max<size_t>(1, outCount);
    if (!m_outData) {
        m_outData = std::make_shared<GpuComplexFloatSignal>(requiredCapacity);
        if (!m_outData || !m_outData->isValid()) {
            ERROR << "Decimator::run failed: unable to allocate output GPU buffer." << std::endl;
            return false;
        }
    } else if (m_outData->availableSize() < requiredCapacity) {
        if (!m_outData->reserve(requiredCapacity) || !m_outData->isValid()) {
            ERROR << "Decimator::run failed: unable to grow output GPU buffer." << std::endl;
            return false;
        }
    }
    if (!m_outData->setLogicalSize(outCount)) {
        ERROR << "Decimator::run failed: unable to set output logical size." << std::endl;
        return false;
    }

    std::string error;
    if (!runDecimatorCuda(
            m_inData->getDeviceData(),
            inputSize,
            m_outData->getDeviceData(),
            m_samplesPerSymbol,
            m_currentPhase,
            &error)) {
        ERROR << error << std::endl;
        return false;
    }
    size_t index = m_currentPhase + (outCount * sps);
    m_currentPhase = index - inputSize;
    if (m_currentPhase >= sps) {
        m_currentPhase %= sps;
    }

    return true;
}

void Decimator::setParam(const std::string& paramName, const std::any& value) {
    if (paramName == "samples per symbol") {
        int parsed = 0;
        if (!anyToInt(value, &parsed)) {
            ERROR << "Decimator::setParam failed: invalid samples per symbol type." << std::endl;
            return;
        }
        m_samplesPerSymbol = parsed;
        return;
    }

    if (paramName == "offset") {
        int parsed = 0;
        if (!anyToInt(value, &parsed)) {
            ERROR << "Decimator::setParam failed: invalid offset type." << std::endl;
            return;
        }
        m_offset = parsed;
        return;
    }

    ERROR << "Decimator::setParam unknown parameter: " << paramName << std::endl;
}

std::shared_ptr<IData> Decimator::getData() {
    return m_outData;
}
