#include <Decimator.hpp>
#include <VariablesResolve.hpp>

#include <module.hpp>

#include <cuda_runtime.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

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
        m_outData = std::make_shared<CpuComplexSignal>();
        return true;
    }

    std::vector<cuComplex> hostInput(inputSize);
    const auto err = cudaMemcpy(
        hostInput.data(),
        m_inData->getDeviceData(),
        inputSize * sizeof(cuComplex),
        cudaMemcpyDeviceToHost
    );
    if (err != cudaSuccess) {
        ERROR << "Decimator::run failed: cudaMemcpy failed: " << cudaGetErrorString(err) << std::endl;
        return false;
    }

    std::vector<cuComplex> out;
    out.reserve((inputSize + static_cast<size_t>(m_samplesPerSymbol) - 1) / static_cast<size_t>(m_samplesPerSymbol));

    size_t index = m_currentPhase;
    while (index < inputSize) {
        out.push_back(hostInput[index]);
        index += static_cast<size_t>(m_samplesPerSymbol);
    }

    m_currentPhase = (index - inputSize);
    if (m_currentPhase >= static_cast<size_t>(m_samplesPerSymbol)) {
        m_currentPhase %= static_cast<size_t>(m_samplesPerSymbol);
    }

    auto* raw = new cuComplex[out.size()];
    std::copy(out.begin(), out.end(), raw);
    m_outData = std::make_shared<CpuComplexSignal>(raw, out.size());
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
