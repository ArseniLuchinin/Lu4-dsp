#include <FIR_filter.hpp>
#include <VariablesResolve.hpp>
#include <CpuFloatSignal.hpp>

#include <cstdint>
#include <cuda_runtime.h>
#include <cuComplex.h>
#include <module.hpp>

// Внешние объявления, что бы не выносить в .hpp файлы
extern __constant__ float d_h[];

namespace {
constexpr int kMaxFIRLength = 2048;

template<typename T>
void cuda_free(T* ptr) {
    if (ptr) {
        cudaFree(ptr);
    }
}

template<typename T>
bool ensureHistoryBuffers(
    const size_t historySize,
    std::shared_ptr<T>& history,
    std::shared_ptr<T>& nextHistory)
{
    if (historySize == 0) {
        history.reset();
        nextHistory.reset();
        return true;
    }

    if (!history) {
        T* ptr = nullptr;
        if (cudaMalloc(&ptr, historySize * sizeof(T)) != cudaSuccess) {
            return false;
        }
        history = std::shared_ptr<T>(ptr, cuda_free<T>);

        if (cudaMemset(history.get(), 0, historySize * sizeof(T)) != cudaSuccess) {
            history.reset();
            return false;
        }
    }

    if (!nextHistory) {
        T* ptr = nullptr;
        if (cudaMalloc(&ptr, historySize * sizeof(T)) != cudaSuccess) {
            return false;
        }
        nextHistory = std::shared_ptr<T>(ptr, cuda_free<T>);
    }

    return true;
}
}

IModule* createModule() {
    return new FIRFilter();
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
    (void)historySize;

    m_signalType = SignalType::None;
    m_data.reset();
    m_floatData.reset();
    m_complexData.reset();

    m_historyFloat.reset();
    m_nextHistoryFloat.reset();
    m_historyComplex.reset();
    m_nextHistoryComplex.reset();

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

    m_floatData = std::dynamic_pointer_cast<GpuFloatSignal>(data);
    m_complexData = std::dynamic_pointer_cast<GpuComplexFloatSignal>(data);
    if (!m_floatData && !m_complexData) {
        ERROR << "Can't handle:" << data->getDataName() << std::endl;
        return false;
    }

    const size_t historySize = static_cast<size_t>(m_M - 1);
    const size_t inputSize = m_floatData ? m_floatData->size() : m_complexData->size();
    if (inputSize < historySize) {
        ERROR << "FIRFilter::setData failed: input size is smaller than history size." << std::endl;
        return false;
    }

    m_data = data;

    if (historySize == 0) {
        m_signalType = m_floatData ? SignalType::Float : SignalType::ComplexFloat;
        return true;
    }

    if (m_floatData) {
        if (!ensureHistoryBuffers(historySize, m_historyFloat, m_nextHistoryFloat)) {
            ERROR << "FIRFilter::setData failed: unable to allocate float history buffers." << std::endl;
            return false;
        }

        const auto err = cudaMemcpy(
            m_nextHistoryFloat.get(),
            m_floatData->getDeviceData() + m_floatData->size() - historySize,
            historySize * sizeof(float),
            cudaMemcpyDeviceToDevice
        );
        if (err != cudaSuccess) {
            ERROR << "FIRFilter::setData failed: cudaMemcpy next float history failed: "
                  << cudaGetErrorString(err) << std::endl;
            return false;
        }

        m_signalType = SignalType::Float;
        return true;
    }

    if (!ensureHistoryBuffers(historySize, m_historyComplex, m_nextHistoryComplex)) {
        ERROR << "FIRFilter::setData failed: unable to allocate complex history buffers." << std::endl;
        return false;
    }

    const auto err = cudaMemcpy(
        m_nextHistoryComplex.get(),
        m_complexData->getDeviceData() + m_complexData->size() - historySize,
        historySize * sizeof(cuComplex),
        cudaMemcpyDeviceToDevice
    );
    if (err != cudaSuccess) {
        ERROR << "FIRFilter::setData failed: cudaMemcpy next complex history failed: "
              << cudaGetErrorString(err) << std::endl;
        return false;
    }

    m_signalType = SignalType::ComplexFloat;
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
