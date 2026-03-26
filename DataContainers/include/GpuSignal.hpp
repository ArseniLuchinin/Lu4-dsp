#ifndef GPU_SIGNAL_HPP
#define GPU_SIGNAL_HPP

#include <IData.hpp>

#include <cstddef>

template<typename T, typename Tag>
class GpuSignal : public IData {
public:
    GpuSignal(const GpuSignal&) = delete;
    GpuSignal& operator=(const GpuSignal&) = delete;

    GpuSignal();
    explicit GpuSignal(size_t size);
    GpuSignal(GpuSignal&& other) noexcept;
    GpuSignal& operator=(GpuSignal&& other) noexcept;
    ~GpuSignal() override;

    bool reserve(const size_t size) override;
    void setDataFromHost(T* data, size_t size);
    void setDataFromDevice(T* data, size_t size);
    bool setLogicalSize(size_t size);

    size_t size() const override;
    size_t availableSize() const override;
    bool isValid() const override;

    T* getDeviceData();

    const char* name() const;

protected:
    void freeData();
    bool checkData(const T* data);

    size_t m_size = 0;
    size_t m_resedSize = 0;
    bool m_siValid = false;

    T* m_data = nullptr;
};

#endif
