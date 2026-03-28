#ifndef CPU_COMPLEX_SIGNAL_HPP
#define CPU_COMPLEX_SIGNAL_HPP

#include <IData.hpp>
#include <GpuComplexSignal.hpp>
#include <cuda_runtime.h>
#include <cuComplex.h>

#include <memory>
#include <string>

class CpuComplexSignal : public IData {
public:
    CpuComplexSignal(const CpuComplexSignal&) = delete;
    CpuComplexSignal& operator=(const CpuComplexSignal&) = delete;

    CpuComplexSignal() : IData("CPU complex signal") {}
    explicit CpuComplexSignal(cuComplex* data, size_t size);

    static std::shared_ptr<CpuComplexSignal> fromGpu(std::shared_ptr<IData> iData);

    ~CpuComplexSignal() override;

    size_t size() const override;
    cuComplex* getData() const;

    size_t availableSize() const override {
        return m_size;
    }

    bool reserve(const size_t /*size*/) override {
        return true;
    }

    bool isValid() const override {
        return m_data != nullptr;
    }

private:
    cuComplex* m_data = nullptr;
    size_t m_size = 0;
};

#endif // CPU_COMPLEX_SIGNAL_HPP
