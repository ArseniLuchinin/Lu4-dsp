#include <FIR_filter.hpp>
#include <VariablesResolve.hpp>
#include <CpuFloatSignal.hpp>

#include <cstdint>
#include <cuda_runtime.h>
#include <module.hpp>

// Внешние объявления, что бы не выносить в .hpp файлы
extern __constant__ float d_h[];

namespace {
constexpr int kMaxFIRLength = 2048;
}

IModule* createModule() {
    return new FIRFilter();
}


void cuda_free(float* ptr) {
    if (ptr) cudaFree(ptr);
}


FIRFilter::FIRFilter() : IModule({"FIR-filter", "", ""}) {}


bool FIRFilter::init()
{
    if (m_M <= 0 || (m_M % 2) == 0) {
        ERROR << "FIRFilter::init failed: filter order must be positive odd number." << std::endl;
        return false;
    }

    if (m_M > kMaxFIRLength) {
        ERROR << "FIRFilter::init failed: filter order exceeds max constant-memory taps." << std::endl;
        return false;
    }

    std::shared_ptr<CpuFloatSignal> rx = std::dynamic_pointer_cast<CpuFloatSignal>(rxData());
    if (!rx) {
        ERROR << "FIRFilter::init failed: coefficients source is not CpuFloatSignal." << std::endl;
        return false;
    }
    if (!rx->isValid()) {
        ERROR << "FIRFilter::init failed: coefficients data is invalid." << std::endl;
        return false;
    }
    if (rx->size() != static_cast<size_t>(m_M)) {
        ERROR << "FIRFilter::init failed: coefficients size must match filter order." << std::endl;
        return false;
    }

    INFO << "rx: " << rx->getDataName() << " size: " << rx->size() << std::endl;

    // Копируем в constant memory
    const auto err = cudaMemcpyToSymbol(d_h, rx->getData(), m_M * sizeof(float));
    if(err != cudaSuccess){
        ERROR << "coefs load failed: " << cudaGetErrorString(err) << std::endl;
        return false;
    }
    INFO << "coefs init" << std::endl;

    const size_t historySize = static_cast<size_t>(m_M - 1);
    if (historySize == 0) {
        m_history.reset();
        m_next_history.reset();
        return true;
    }

    float* ptr = nullptr;
    if (cudaMalloc(&ptr, historySize * sizeof(float)) != cudaSuccess) {
        ERROR << "FIRFilter::init failed: cudaMalloc for history failed." << std::endl;
        return false;
    }
    m_history = std::shared_ptr<float>(ptr, cuda_free);
    if (cudaMemset(m_history.get(), 0, historySize * sizeof(float)) != cudaSuccess) {
        ERROR << "FIRFilter::init failed: cudaMemset for history failed." << std::endl;
        return false;
    }

    float* nextHistoryPtr = nullptr;
    if (cudaMalloc(&nextHistoryPtr, historySize * sizeof(float)) != cudaSuccess) {
        ERROR << "FIRFilter::init failed: cudaMalloc for next history failed." << std::endl;
        return false;
    }
    m_next_history = std::shared_ptr<float>(nextHistoryPtr, cuda_free);

    return true;
}

bool FIRFilter::setData(std::shared_ptr<IData> data){
    if (!data) {
        ERROR << "Input data is nullptr." << std::endl;
        return false;
    }

    if (m_M <= 0) {
        ERROR << "FIRFilter::setData failed: filter order is not initialized." << std::endl;
        return false;
    }

    m_data = std::dynamic_pointer_cast<GpuFloatSignal>(data);
    if(not m_data) {
        ERROR << "Can't handle:" << data->getDataName() << std::endl;
        return false;
    }

    const size_t historySize = static_cast<size_t>(m_M - 1);
    if (m_data->size() < historySize) {
        ERROR << "FIRFilter::setData failed: input size is smaller than history size." << std::endl;
        return false;
    }

    if (historySize == 0) {
        return true;
    }

    const auto err = cudaMemcpy(
        m_next_history.get(),
        m_data->getDeviceData() + m_data->size() - historySize,
        historySize * sizeof(float),
        cudaMemcpyDeviceToDevice
    );
    if (err != cudaSuccess) {
        ERROR << "FIRFilter::setData failed: cudaMemcpy next history failed: "
              << cudaGetErrorString(err) << std::endl;
        return false;
    }

    return true;
}

void FIRFilter::setParam(const std::string& paramName, const std::any& value) {
    const std::any resolved = resolveParamValue(value);
    if(paramName == "coefficients data tag"){
        setTag(std::any_cast<std::string>(resolved));
        return;
    }

    if (paramName == "filter order") {
        m_M = std::any_cast<int32_t>(resolved);
        return;
    }

    ERROR << "can't handle param: " << paramName << std::endl;
}


std::shared_ptr<IData> FIRFilter::getData()
{
    return m_data;
}
