#include <GpuFloatSignal.hpp>
#include <cuda_runtime.h>

GpuFloatSignal::GpuFloatSignal(size_t size) {
    m_size = size;
    cudaMalloc(&m_data, sizeof(float) * m_size);
}

void GpuFloatSignal::setDataFromHost(float* data, size_t size) {
    m_size = size;
    cudaMemcpy(m_data, data, sizeof(float) * m_size, cudaMemcpyHostToDevice);
}

void GpuFloatSignal::setDataFromDevice(float* data, const size_t size) {

    m_size = size;
    cudaMemcpy(m_data, data, sizeof(float) * m_size, cudaMemcpyDeviceToDevice);
}


GpuFloatSignal::GpuFloatSignal(GpuFloatSignal && other) {
    m_size = other.m_size;
    m_data = other.m_data;
    other.m_size = 0;
    other.m_data = nullptr;
}

GpuFloatSignal::~GpuFloatSignal() {
    freeData();
}

float* GpuFloatSignal::getDeviceData() {
    return m_data;
}

void GpuFloatSignal::freeData() {
    if(m_data != nullptr) {
        m_size = 0;
        cudaFree(m_data);
    }
}