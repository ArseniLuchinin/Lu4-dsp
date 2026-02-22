#include "CpuFloatSignal.hpp"
#include <iostream>

// Конструктор
CpuFloatSignal::CpuFloatSignal(float* data, size_t size) :
    m_data(data),
    m_size(size)
{
}

// Статический фабричный метод fromGpu
CpuFloatSignal CpuFloatSignal::fromGpu(std::shared_ptr<IData> iData) {
    auto gpuData = std::dynamic_pointer_cast<GpuFloatSignal>(iData);
    if(gpuData == nullptr) {
        std::cerr << "CpuFloatSignal::fromGpu: Input IData is not a GpuFloatSignal." << std::endl;
        return CpuFloatSignal();
    }

    if (gpuData->size() == 0) {
        std::cerr << "CpuFloatSignal::fromGpu: GpuFloatSignal has size 0." << std::endl;
        return CpuFloatSignal();
    }

    float* data = new float[gpuData->size()];
    cudaError_t err = cudaMemcpy(data, gpuData->getDeviceData(), gpuData->size() * sizeof(float), cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        std::cerr << "CpuFloatSignal::fromGpu: cudaMemcpy failed: " << cudaGetErrorString(err) << std::endl;
        delete[] data;
        return CpuFloatSignal();
    }
    return CpuFloatSignal(data, gpuData->size());
}

// Деструктор
CpuFloatSignal::~CpuFloatSignal() {
    if (m_data) {
        delete[] m_data;
        m_data = nullptr;
    }
}

size_t CpuFloatSignal::size() const {
    return m_size;
}

std::string CpuFloatSignal::getDataName() const {
    return "Cpu Float Signal";
}

float* CpuFloatSignal::getData() const {
    return m_data;
}
