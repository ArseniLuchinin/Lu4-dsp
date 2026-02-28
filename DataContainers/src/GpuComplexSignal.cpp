#include <GpuComplexSignal.hpp>

#include <cuda_runtime.h>

GpuComplexFloatSignal::GpuComplexFloatSignal(const cuComplex* data, size_t size)
    : IData("Gpu complex float signal"), m_data(nullptr), m_size(size) {
    const auto err = cudaMalloc((void**)&m_data, sizeof(cuComplex) * m_size);
    if (err != cudaSuccess) {
        ERROR << "Failed to allocate memory on GPU" << std::endl;
        m_data = nullptr;
        m_size = 0;
        return;
    }

    if (data != nullptr && m_size > 0) {
        cudaMemcpy(m_data, data, sizeof(cuComplex) * m_size, cudaMemcpyHostToDevice);
        INFO << "Data copied from host to device" << std::endl;
    }
}

GpuComplexFloatSignal::GpuComplexFloatSignal(GpuComplexFloatSignal&& other)
    : IData("Gpu complex float signal"), m_data(other.m_data), m_size(other.m_size) {
    other.m_data = nullptr;
    other.m_size = 0;
}

GpuComplexFloatSignal::~GpuComplexFloatSignal() {
    freeData();
}

void GpuComplexFloatSignal::setDataFromHost(cuComplex* data, size_t size) {
    if(not checkData(data))
        return;

    if(size != m_size){
        ERROR << "GPU data size is smaller than host data";
        freeData();
        return;
    }

    const auto err = cudaMemcpy(m_data, data, sizeof(cuComplex) * m_size, cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        ERROR << "Failed to allocate memory on GPU" << std::endl;
        freeData();
        return;
    }
}

void GpuComplexFloatSignal::setDataFromDevice(cuComplex* data, const size_t size) {
    if(not checkData(data))
        return;

    if(size != m_size){
        ERROR << "GPU data size is smaller than host data";
        freeData();
        return;
    }
    
    const auto err = cudaMemcpy(m_data, data, sizeof(cuComplex) * m_size, cudaMemcpyDeviceToDevice);
    if (err != cudaSuccess) {
        ERROR << "Failed to allocate memory on GPU" << std::endl;
        freeData();
        return;
    }
}

cuComplex* GpuComplexFloatSignal::getDeviceData() {
    return (m_data);
}

size_t GpuComplexFloatSignal::size() const {
    return m_size;
}


void GpuComplexFloatSignal::freeData() {
    if(m_data != nullptr) {
        m_size = 0;
        cudaFree(m_data);
        m_data = nullptr;
    }
}

bool GpuComplexFloatSignal::checkData(const cuComplex* data) {
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