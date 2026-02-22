#ifndef CPU_FLOAT_SIGNAL_HPP
#define CPU_FLOAT_SIGNAL_HPP

#include <IData.hpp>
#include <GpuFloatSignal.hpp>
#include <cuda_runtime.h>
#include <memory>
#include <string>

class CpuFloatSignal : public IData {
public:
    CpuFloatSignal() = default;
    explicit CpuFloatSignal(float* data, size_t size);

    static CpuFloatSignal fromGpu(std::shared_ptr<IData> iData);

    virtual ~CpuFloatSignal();

    size_t size() const override;

    std::string getDataName() const override;

    float* getData() const;

protected:
    float* m_data = nullptr;
    size_t m_size = 0;
};

#endif // CPU_FLOAT_SIGNAL_HPP