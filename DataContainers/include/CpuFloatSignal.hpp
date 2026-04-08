#ifndef CPU_FLOAT_SIGNAL_HPP
#define CPU_FLOAT_SIGNAL_HPP

#include <IData.hpp>
#include <GpuFloatSignal.hpp>
#include <cuda_runtime.h>
#include <memory>
#include <string>

class CpuFloatSignal : public IData {
public:
    CpuFloatSignal(const CpuFloatSignal&) = delete;
    CpuFloatSignal& operator=(const CpuFloatSignal&) = delete;

    CpuFloatSignal() : IData("CPU float signal") {}
    explicit CpuFloatSignal(float* data, size_t size);

    static std::shared_ptr<CpuFloatSignal> fromGpu(std::shared_ptr<IData> iData);

    virtual ~CpuFloatSignal();

    size_t size() const override;

    float* getData() const;

    size_t availableSize() const override {
        return m_size;
    }

    bool reserve(const size_t size) override {
        return true;
    }

    bool isValid() const override {
        return m_data != nullptr;
    }

    std::shared_ptr<IData> copy() const override;

protected:
    float* m_data = nullptr;
    size_t m_size = 0;
};

#endif // CPU_FLOAT_SIGNAL_HPP