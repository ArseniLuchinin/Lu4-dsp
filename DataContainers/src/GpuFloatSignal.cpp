#include <GpuFloatSignal.hpp>
#include <cuda_runtime.h>

GpuFloatSignal::GpuFloatSignal(size_t size) : IData("Gpu float signal") {
    m_size = size;
    const auto err = cudaMalloc((void**)&m_data, sizeof(float) * m_size);
    if(err != cudaSuccess) {
        ERROR << "Failed to reseve memory on GPU to: " << m_size << std::endl;
        m_size = 0;
        m_data = nullptr;
    }
}

void GpuFloatSignal::setDataFromHost(float* data, size_t size) {
    if(size > m_size){
        ERROR << "GPU data size is smaller than host data";
        freeData();        return;
    }

    if(not checkData(data))
        return;

    const auto err = cudaMemcpy(m_data, data, sizeof(float) * m_size, cudaMemcpyHostToDevice);

    if(err != cudaSuccess){
        ERROR << "Failed to copy from CPU to  GPU: " << cudaGetErrorString(err) << std::endl;
        freeData();
        return;
    }
}

void GpuFloatSignal::setDataFromDevice(float* data, const size_t size) {
    if(size > m_size){
        ERROR << "GPU data size is smaller than host data";
        return;
    }

    if(not checkData(data))
        return;

    const auto err = cudaMemcpy(m_data, data, sizeof(float) * m_size, cudaMemcpyDeviceToDevice);

    if(err != cudaSuccess){
        ERROR << "Failed to copy from GPU to  GPU: " << cudaGetErrorString(err) << std::endl;
        freeData();
        return;
    }
}


GpuFloatSignal::GpuFloatSignal(GpuFloatSignal && other) : IData("Gpu float signal") {
    m_size = other.m_size;
    m_data = other.m_data;
    other.m_size = 0;
    other.m_data = nullptr;
}

GpuFloatSignal::~GpuFloatSignal() {
    freeData();
    DEBUG << "BB" << std::endl;
}

float* GpuFloatSignal::getDeviceData() {
    return m_data;
}

void GpuFloatSignal::freeData() {
    if(m_data != nullptr) {
        m_size = 0;
        cudaFree(m_data);
        m_data = nullptr;
    }
}

bool GpuFloatSignal::checkData(const float* data) {
    if(not data) {
        ERROR << "Data is nullptr" << std::endl;
        return false;
    }

    if(not m_data) {
        ERROR << "GPU data corrupted to nullptr" << std::endl;
        return false;
    }

    return true;
}