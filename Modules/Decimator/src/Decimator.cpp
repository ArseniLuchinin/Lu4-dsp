#include <Decimator.hpp>
#include <VariablesResolve.hpp>

#include <module.hpp>

#include <cuda_runtime.h>

#include <algorithm>
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

    if (value.type() == typeid(int32_t)) {
        *out = static_cast<int>(std::any_cast<int32_t>(value));
        return true;
    }
    if (value.type() == typeid(int64_t)) {
        *out = static_cast<int>(std::any_cast<int64_t>(value));
        return true;
    }
    if (value.type() == typeid(uint32_t)) {
        *out = static_cast<int>(std::any_cast<uint32_t>(value));
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

    const auto syncErr = cudaDeviceSynchronize();
    if (syncErr != cudaSuccess) {
        if (error) {
            *error = std::string("Decimator::run failed: kernel execution failed: ") + cudaGetErrorString(syncErr);
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
    : IModule({"Decimator", "", ""})
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

    auto out = std::make_shared<GpuComplexFloatSignal>(std::max<size_t>(1, outCount));
    if (!out || !out->isValid()) {
        ERROR << "Decimator::run failed: unable to allocate output GPU buffer." << std::endl;
        return false;
    }
    if (!out->setLogicalSize(outCount)) {
        ERROR << "Decimator::run failed: unable to set output logical size." << std::endl;
        return false;
    }

    std::string error;
    if (!runDecimatorCuda(
            m_inData->getDeviceData(),
            inputSize,
            out->getDeviceData(),
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

    m_outData = out;
    return true;
}

void Decimator::setParam(const std::string& paramName, const std::any& value) {
    const std::any resolved = resolveParamValue(value);

    if (paramName == "samples per symbol") {
        int parsed = 0;
        if (!anyToInt(resolved, &parsed)) {
            ERROR << "Decimator::setParam failed: invalid samples per symbol type." << std::endl;
            return;
        }
        m_samplesPerSymbol = parsed;
        return;
    }

    if (paramName == "offset") {
        int parsed = 0;
        if (!anyToInt(resolved, &parsed)) {
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
