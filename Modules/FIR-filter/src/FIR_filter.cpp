#include <FIR_filter.hpp>
#include <CpuFloatSignal.hpp>

#include <cmath>
#include <vector>
#include <stdexcept>
#include <cstdint>
#include <cuda_runtime.h>
#include <module.hpp>
#include <iostream>

// Внешние объявления, что бы не выносить в .hpp файлы
extern __constant__ float d_h[];
extern const int MAX_FIR_LENGTH;

IModule* createModule() {
    return new FIRFilter();
}


void cuda_free(float* ptr) {
    if (ptr) cudaFree(ptr);
}


FIRFilter::FIRFilter() : IModule({"FIR-filter", "", ""}) {}


bool FIRFilter::init()
{
    std::shared_ptr<CpuFloatSignal> rx = std::dynamic_pointer_cast<CpuFloatSignal>(rxData());
    INFO << "rx: " << rx->getDataName() << " size: " << rx->size() << std::endl;

    // Копируем в constant memory
    const auto err = cudaMemcpyToSymbol(d_h, rx->getData(), m_M * sizeof(float));
    if(err != cudaSuccess){
        ERROR << "coefs load failed: " << cudaGetErrorString(err) << std::endl;
        return false;
    }
    INFO << "coefs init" << std::endl;

    float* ptr; 
    cudaMalloc(&ptr, (m_M - 1) * sizeof(float));
    m_history = std::shared_ptr<float>(ptr, cuda_free);
    cudaMemset(m_history.get(), 0, (m_M - 1) * sizeof(float));

    float* nextHistoryPtr;
    cudaMalloc(&nextHistoryPtr, (m_M - 1) * sizeof(float));
    m_next_history = std::shared_ptr<float>(nextHistoryPtr, cuda_free);

    return true;
}

bool FIRFilter::setData(std::shared_ptr<IData> data){
    if (!data) {
        ERROR << "Input data is nullptr." << std::endl;
        return false;
    }

    m_data = std::dynamic_pointer_cast<GpuFloatSignal>(data);
    if(not m_data) {
        ERROR << "Can't handle:" << data->getDataName() << std::endl;
        return false;
    }

    const size_t historySize = m_M - 1;

    cudaMemcpy(
        m_next_history.get(),
        m_data->getDeviceData() + m_data->size() - historySize,
        historySize * sizeof(float),
        cudaMemcpyDeviceToDevice
    );

    return true;
}

void FIRFilter::setParam(const std::string& paramName, const std::any& value) {
    if(paramName == "coefficients data tag"){
        setTag(std::any_cast<std::string>(value));
        return;
    }

    if (paramName == "filter order") {
        m_M = std::any_cast<int32_t>(value);
        return;
    }

    ERROR << "can't handle param: " << paramName << std::endl;
}


std::shared_ptr<IData> FIRFilter::getData()
{
    return m_data;
}
