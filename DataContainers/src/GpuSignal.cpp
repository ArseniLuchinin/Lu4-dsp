#include <GpuSignal.hpp>

#include <cuda_runtime.h>

template<typename T, typename Tag>
GpuSignal<T, Tag>::GpuSignal() : IData(name()) {}

template<typename T, typename Tag>
GpuSignal<T, Tag>::GpuSignal(size_t size) : IData(name()), m_size(size) {
    const auto err = cudaMalloc((void**)&m_data, sizeof(T) * m_size);
    if (err != cudaSuccess) {
        ERROR << "Failed to reserve memory on GPU: " << m_size << std::endl;
        m_size = 0;
        m_data = nullptr;
        m_resedSize = 0;
        m_siValid = false;
        return;
    }
    m_resedSize = m_size;
    m_siValid = true;
}

template<typename T, typename Tag>
GpuSignal<T, Tag>::GpuSignal(GpuSignal&& other) noexcept : IData(name()) {
    m_size = other.m_size;
    m_resedSize = other.m_resedSize;
    m_siValid = other.m_siValid;
    m_data = other.m_data;
    other.m_size = 0;
    other.m_resedSize = 0;
    other.m_siValid = false;
    other.m_data = nullptr;
}

template<typename T, typename Tag>
GpuSignal<T, Tag>& GpuSignal<T, Tag>::operator=(GpuSignal&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    freeData();
    m_size = other.m_size;
    m_resedSize = other.m_resedSize;
    m_siValid = other.m_siValid;
    m_data = other.m_data;
    other.m_size = 0;
    other.m_resedSize = 0;
    other.m_siValid = false;
    other.m_data = nullptr;
    return *this;
}

template<typename T, typename Tag>
GpuSignal<T, Tag>::~GpuSignal() {
    freeData();
}

template<typename T, typename Tag>
bool GpuSignal<T, Tag>::reserve(const size_t size) {
    if (size == 0) {
        freeData();
        m_siValid = false;
        return true;
    }

    if (m_data != nullptr && size <= m_resedSize) {
        DEBUG << "Don't down malloc gpu momory. Recreat plz";
        m_siValid = true;
        return true;
    }

    freeData();

    const auto err = cudaMalloc((void**)&m_data, sizeof(T) * size);
    if (err != cudaSuccess) {
        ERROR << "Failed to reserve memory on GPU: " << size << std::endl;
        m_size = 0;
        m_resedSize = 0;
        m_siValid = false;
        m_data = nullptr;
        return false;
    }

    m_resedSize = size;
    m_siValid = true;
    return true;
}

template<typename T, typename Tag>
void GpuSignal<T, Tag>::setDataFromHost(T* data, size_t size) {
    if (size > m_resedSize) {
        ERROR << "GPU data size is smaller than host data";
        freeData();
        return;
    }

    if (!checkData(data)) {
        return;
    }

    const auto err = cudaMemcpy(m_data, data, sizeof(T) * size, cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        ERROR << "Failed to copy from CPU to GPU: " << cudaGetErrorString(err) << std::endl;
        freeData();
        return;
    }
    m_size = size;
    m_siValid = true;
}

template<typename T, typename Tag>
void GpuSignal<T, Tag>::setDataFromDevice(T* data, size_t size) {
    if (size > m_resedSize) {
        ERROR << "GPU data size is smaller than source device data";
        return;
    }

    if (!checkData(data)) {
        return;
    }

    const auto err = cudaMemcpy(m_data, data, sizeof(T) * size, cudaMemcpyDeviceToDevice);
    if (err != cudaSuccess) {
        ERROR << "Failed to copy from GPU to GPU: " << cudaGetErrorString(err) << std::endl;
        freeData();
        return;
    }
    m_size = size;
    m_siValid = true;
}

template<typename T, typename Tag>
bool GpuSignal<T, Tag>::setLogicalSize(size_t size) {
    if (size > m_resedSize) {
        ERROR << "GPU data size is smaller than requested logical size";
        return false;
    }
    m_size = size;
    m_siValid = (m_data != nullptr);
    return m_siValid;
}

template<typename T, typename Tag>
size_t GpuSignal<T, Tag>::size() const {
    return m_size;
}

template<typename T, typename Tag>
size_t GpuSignal<T, Tag>::availableSize() const {
    return m_resedSize;
}

template<typename T, typename Tag>
bool GpuSignal<T, Tag>::isValid() const {
    return m_siValid;
}

template<typename T, typename Tag>
T* GpuSignal<T, Tag>::getDeviceData() {
    return m_data;
}

template<typename T, typename Tag>
const char* GpuSignal<T, Tag>::name() const
{
    return Tag::name;
}

template<typename T, typename Tag>
void GpuSignal<T, Tag>::freeData() {
    if (m_data != nullptr) {
        m_size = 0;
        m_resedSize = 0;
        m_siValid = false;
        cudaFree(m_data);
        m_data = nullptr;
    }
}

template<typename T, typename Tag>
bool GpuSignal<T, Tag>::checkData(const T* data) {
    if (!data) {
        ERROR << "Data is nullptr" << std::endl;
        return false;
    }

    if (!m_data) {
        ERROR << "GPU data corrupted to nullptr" << std::endl;
        return false;
    }

    return true;
}

#include <GpuFloatSignal.hpp>
#include <GpuComplexSignal.hpp>

template class GpuSignal<float, gpu_float_tag>;
template class GpuSignal<cuComplex, gpu_comples_float_tag>;
