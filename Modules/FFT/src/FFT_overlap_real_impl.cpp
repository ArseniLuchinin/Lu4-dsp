#include "FFT_overlap_impl.hpp"
#include "FFT_overlap_callback.hpp"

#include <cuda_runtime.h>

namespace {

bool updateRealOverlapBuffer(float* inputPtr,
                             int batchCount,
                             size_t hopSize,
                             size_t overlapSize,
                             bool isFirstRun,
                             const std::shared_ptr<GpuFloatSignal>& buffer)
{
    if (overlapSize == 0) {
        return true;
    }

    const size_t tailStart = isFirstRun
        ? static_cast<size_t>(batchCount) * hopSize
        : static_cast<size_t>(batchCount) * hopSize - overlapSize;

    buffer->setDataFromDevice(inputPtr + tailStart, overlapSize);
    return buffer->isValid();
}

} // namespace

bool RealFFTOverlapImpl::setData(std::shared_ptr<IData> data)
{
    m_inData = std::dynamic_pointer_cast<GpuFloatSignal>(data);
    if (!m_inData) {
        m_lastError = "RealFFTOverlapImpl::setData: input is not GpuFloatSignal.";
        return false;
    }

    return true;
}

bool RealFFTOverlapImpl::ensureOutputForBatch(int batchCount, size_t fftSize)
{
    const size_t outputSize = outputPerBatch(fftSize) * static_cast<size_t>(batchCount);
    if (!m_outData || m_outData->availableSize() < outputSize) {
        m_outData = std::make_shared<GpuComplexFloatSignal>(outputSize);
    }

    if (!m_outData || !m_outData->isValid()) {
        m_lastError = "RealFFTOverlapImpl::ensureOutputForBatch: failed to allocate output buffer.";
        return false;
    }

    return true;
}

size_t RealFFTOverlapImpl::inputSize() const
{
    return m_inData ? m_inData->size() : 0;
}

size_t RealFFTOverlapImpl::outputPerBatch(size_t fftSize) const
{
    return (fftSize / 2) + 1;
}

cufftType RealFFTOverlapImpl::planType() const
{
    return CUFFT_R2C;
}

bool RealFFTOverlapImpl::execute(cufftHandle plan,
                                 size_t fftSize,
                                 size_t hopSize,
                                 size_t overlapSize,
                                 bool isFirstRun,
                                 int batchCount)
{
    if (!m_inData || !m_outData) {
        m_lastError = "RealFFTOverlapImpl::execute: buffers are not initialized.";
        return false;
    }

    if (!m_overlapBuffer) {
        m_overlapBuffer = std::make_shared<GpuFloatSignal>(overlapSize);
    }
    if (!m_overlapBuffer || !m_overlapBuffer->isValid()) {
        m_lastError = "RealFFTOverlapImpl::execute: failed to allocate overlap buffer.";
        return false;
    }

    auto* inPtr = m_inData->getDeviceData();
    auto* signalPtr = isFirstRun ? (inPtr + overlapSize) : inPtr;
    auto* outPtr = reinterpret_cast<cufftComplex*>(m_outData->getDeviceData());

    if (isFirstRun && overlapSize > 0) {
        m_overlapBuffer->setDataFromDevice(inPtr, overlapSize);
        if (!m_overlapBuffer->isValid()) {
            m_lastError = "RealFFTOverlapImpl::execute: failed to initialize overlap buffer.";
            return false;
        }
    }

    void* callbackData = nullptr;
    if (!setupOverlapLoadCallback(plan,
                                  signalPtr,
                                  m_overlapBuffer->getDeviceData(),
                                  static_cast<int>(fftSize),
                                  static_cast<int>(overlapSize),
                                  static_cast<int>(hopSize),
                                  &callbackData,
                                  &m_lastError)) {
        return false;
    }

    const auto execStatus = cufftExecR2C(plan, inPtr, outPtr);
    if (execStatus != CUFFT_SUCCESS) {
        m_lastError = "RealFFTOverlapImpl::execute: cufftExecR2C failed.";
        releaseOverlapLoadCallback(callbackData);
        return false;
    }

    const auto cudaErr = cudaGetLastError();
    if (cudaErr != cudaSuccess) {
        m_lastError = std::string("RealFFTOverlapImpl::execute: CUDA execution error: ") +
                      cudaGetErrorString(cudaErr);
        releaseOverlapLoadCallback(callbackData);
        return false;
    }

    if (!updateRealOverlapBuffer(inPtr,
                                 batchCount,
                                 hopSize,
                                 overlapSize,
                                 isFirstRun,
                                 m_overlapBuffer)) {
        m_lastError = "RealFFTOverlapImpl::execute: failed to update overlap buffer.";
        releaseOverlapLoadCallback(callbackData);
        return false;
    }

    releaseOverlapLoadCallback(callbackData);
    return true;
}

std::shared_ptr<IData> RealFFTOverlapImpl::getData()
{
    return m_outData;
}

const std::string& RealFFTOverlapImpl::lastError() const
{
    return m_lastError;
}
