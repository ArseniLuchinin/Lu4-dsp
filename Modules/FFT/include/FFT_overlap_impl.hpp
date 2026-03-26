#ifndef FFT_OVERLAP_IMPL_HPP
#define FFT_OVERLAP_IMPL_HPP

#include <IData.hpp>
#include <GpuFloatSignal.hpp>
#include <GpuComplexSignal.hpp>

#include <cufft.h>

#include <memory>
#include <string>

class IFFTOverlapImpl {
public:
    virtual ~IFFTOverlapImpl() = default;

    virtual bool setData(std::shared_ptr<IData> data) = 0;
    virtual bool ensureOutputForBatch(int batchCount, size_t fftSize) = 0;
    virtual size_t inputSize() const = 0;
    virtual size_t outputPerBatch(size_t fftSize) const = 0;
    virtual cufftType planType() const = 0;
    virtual bool execute(cufftHandle plan,
                         size_t fftSize,
                         size_t hopSize,
                         size_t overlapSize,
                         bool isFirstRun,
                         int batchCount) = 0;
    virtual std::shared_ptr<IData> getData() = 0;
    virtual const std::string& lastError() const = 0;
};

class RealFFTOverlapImpl : public IFFTOverlapImpl {
public:
    bool setData(std::shared_ptr<IData> data) override;
    bool ensureOutputForBatch(int batchCount, size_t fftSize) override;
    size_t inputSize() const override;
    size_t outputPerBatch(size_t fftSize) const override;
    cufftType planType() const override;
    bool execute(cufftHandle plan,
                 size_t fftSize,
                 size_t hopSize,
                 size_t overlapSize,
                 bool isFirstRun,
                 int batchCount) override;
    std::shared_ptr<IData> getData() override;
    const std::string& lastError() const override;

private:
    std::shared_ptr<GpuFloatSignal> m_inData;
    std::shared_ptr<GpuFloatSignal> m_overlapBuffer;
    std::shared_ptr<GpuComplexFloatSignal> m_outData;
    std::string m_lastError;
};

class ComplexFFTOverlapImpl : public IFFTOverlapImpl {
public:
    bool setData(std::shared_ptr<IData> data) override;
    bool ensureOutputForBatch(int batchCount, size_t fftSize) override;
    size_t inputSize() const override;
    size_t outputPerBatch(size_t fftSize) const override;
    cufftType planType() const override;
    bool execute(cufftHandle plan,
                 size_t fftSize,
                 size_t hopSize,
                 size_t overlapSize,
                 bool isFirstRun,
                 int batchCount) override;
    std::shared_ptr<IData> getData() override;
    const std::string& lastError() const override;

private:
    std::shared_ptr<GpuComplexFloatSignal> m_inData;
    std::shared_ptr<GpuComplexFloatSignal> m_overlapBuffer;
    std::shared_ptr<GpuComplexFloatSignal> m_outData;
    std::string m_lastError;
};

#endif
