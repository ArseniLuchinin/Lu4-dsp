#include "SumReduce.hpp"
#include <fstream>
#include <iostream>
#include <cstring>

#include <cub/cub.cuh>
#include <cuda_runtime.h>

#include <module.hpp>

IModule* createModule() {
    return new SumReduce();
}

// Вызывается после заполнения значений
bool SumReduce::init() {
    return true;
}

bool SumReduce::run() {
    if (!m_data || m_data->size() == 0) {
        return false;  // или выбросить исключение
    }
    
    size_t temp_storage_bytes = 0;
    float* d_in = m_data->getDeviceData();
    size_t n = m_data->size();
    
    float* d_out = nullptr;
    cudaMalloc(&d_out, sizeof(float));
    
    cub::DeviceReduce::Sum(nullptr, temp_storage_bytes, d_in, d_out, n);
    
    float* d_temp_storage = nullptr;
    cudaMalloc(&d_temp_storage, temp_storage_bytes);
    
    cub::DeviceReduce::Sum(d_temp_storage, temp_storage_bytes, d_in, d_out, n);
    
    cudaFree(d_temp_storage);
    
    // Данные сводятся к одному числу, незачем хранить лишнее
    m_data = std::make_shared<GpuFloatSignal>(1);
    m_data->setDataFromDevice(d_out, 1); 
    
    return true;
}
void SumReduce::setParam(const std::string& paramName, const std::string& value) {
}

bool SumReduce::setData(std::shared_ptr<IData> data) {
    m_data = std::dynamic_pointer_cast<GpuFloatSignal>(data);
    return m_data != nullptr;
}

std::shared_ptr<IData> SumReduce::getData() {
    return m_data;
}