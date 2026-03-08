#include <FIR_filter.hpp>

#include <cmath>
#include <vector>
#include <stdexcept>
#include <cstdint>
#include <cuda_runtime.h>
#include <module.hpp>

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
    m_coeff.resize(m_M);

    float f1 = m_F1 / m_Fs;
    float f2 = m_F2 / m_Fs;

    int mid = (m_M - 1) / 2;

    for (int i = 0; i < m_M; ++i)
    {
        int k = i - mid;

        float val;

        if (k == 0)
            val = 2.0f * (f2 - f1);
        else
            val = (sinf(2.0f * M_PI * f2 * k)
                 - sinf(2.0f * M_PI * f1 * k))
                 / (M_PI * k);

        float w = 0.54f - 0.46f *
                  cosf(2.0f * M_PI * i / (m_M - 1));

        m_coeff[i] = val * w;
    }

    // Копируем в constant memory
    cudaMemcpyToSymbol(d_h, m_coeff.data(), m_M * sizeof(float));
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
    if (paramName == "sample rate") {
        m_Fs = std::any_cast<int32_t>(value);
        return;
    }

    if (paramName == "low cutoff") {
        m_F1 = std::any_cast<float>(value);
        return;
    }

    if (paramName == "high cutoff") {
        m_F2 = std::any_cast<float>(value);
        return;
    }

    if (paramName == "filter order") {
        m_M = std::any_cast<int32_t>(value);
        return;
    }

    if (paramName == "block size") {
        m_blockSize = std::any_cast<int32_t>(value);
        return;
    }

    ERROR << "can't handle param: " << paramName << std::endl;
}


std::shared_ptr<IData> FIRFilter::getData()
{
    return m_data;
}
