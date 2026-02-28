#ifndef GPU_SIGNAL_HPP
#define GPU_SIGNAL_HPP

#include <IData.hpp>

#include <cuda_runtime.h>

#include <cstddef>
#include <string>
#include <type_traits>

template<typename T, typename Tag>
class GpuSignal : public IData {
public:
    GpuSignal(const GpuSignal&) = delete;
    GpuSignal& operator=(const GpuSignal&) = delete;

    GpuSignal() : IData(name()) {}

    explicit GpuSignal(const size_t size) : IData(name()), m_size(size) {
        const auto err = cudaMalloc((void**)&m_data, sizeof(T) * m_size);
        if (err != cudaSuccess) {
            ERROR << "Failed to reserve memory on GPU: " << m_size << std::endl;
            m_size = 0;
            m_data = nullptr;
        }
    }

    GpuSignal(GpuSignal&& other) noexcept : IData(name()) {
        m_size = other.m_size;
        m_data = other.m_data;
        other.m_size = 0;
        other.m_data = nullptr;
    }

    GpuSignal& operator=(GpuSignal&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        freeData();
        m_size = other.m_size;
        m_data = other.m_data;
        other.m_size = 0;
        other.m_data = nullptr;
        return *this;
    }

    ~GpuSignal() override {
        freeData();
    }

    void setDataFromHost(T* data, const size_t size) {
        if (size > m_size) {
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
        }
    }

    void setDataFromDevice(T* data, const size_t size) {
        if (size > m_size) {
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
        }
    }

    size_t size() const override {
        return m_size;
    }

    T* getDeviceData() {
        return m_data;
    }

    const char* name() const
    {
        return Tag::name;
    }

protected:
    void freeData() {
        if (m_data != nullptr) {
            m_size = 0;
            cudaFree(m_data);
            m_data = nullptr;
        }
    }

    bool checkData(const T* data) {
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

    size_t m_size = 0;
    T* m_data = nullptr;
};

#endif
