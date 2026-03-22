#include "FFT_overlap_callback.hpp"

#include <cufftXt.h>
#include <cuda_runtime.h>

#include <iostream>

namespace {

template<typename T>
struct CallbackData {
    T* signal;
    T* buffer;
    int fftSize;
    int overlap;
    int hop;
};

__device__ cufftReal overlap_load_cb_real(void* dataIn,
                                          size_t offset,
                                          void* callerInfo,
                                          void* sharedPtr)
{
    (void)dataIn;
    (void)sharedPtr;

    auto* cb = static_cast<CallbackData<cufftReal>*>(callerInfo);

    const int fftSize = cb->fftSize;
    const int overlap = cb->overlap;
    const int hop = cb->hop;

    const int batch = static_cast<int>(offset / fftSize);
    const int i = static_cast<int>(offset % fftSize);

    const int windowStart = batch * hop - overlap;
    const int signalIdx = windowStart + i;

    return (signalIdx >= 0)
        ? cb->signal[signalIdx]
        : cb->buffer[signalIdx + overlap];
}

__device__ cufftComplex overlap_load_cb(void* dataIn,
                                        size_t offset,
                                        void* callerInfo,
                                        void* sharedPtr)
{
    (void)dataIn;
    (void)sharedPtr;

    auto* cb = static_cast<CallbackData<cufftComplex>*>(callerInfo);

    const int fftSize = cb->fftSize;
    const int overlap = cb->overlap;
    const int hop = cb->hop;

    const int batch = static_cast<int>(offset / fftSize);
    const int i = static_cast<int>(offset % fftSize);

    const int windowStart = batch * hop - overlap;
    const int signalIdx = windowStart + i;

    return (signalIdx >= 0)
        ? cb->signal[signalIdx]
        : cb->buffer[signalIdx + overlap];
}

__device__ cufftCallbackLoadR overlap_load_cb_real_ptr = overlap_load_cb_real;
__device__ cufftCallbackLoadC overlap_load_cb_ptr = overlap_load_cb;

} // namespace

template<typename T>
bool allocateAndCopyCallbackData(T* signal,
                                 T* buffer,
                                 int fftSize,
                                 int overlap,
                                 int hop,
                                 void** callbackData,
                                 std::string* errorMessage)
{
    auto setError = [&](const std::string& message) {
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        std::cerr << message << std::endl;
    };

    if (callbackData == nullptr) {
        setError("setupOverlapLoadCallback: callbackData is null.");
        return false;
    }

    CallbackData<T> hostData{
        signal,
        buffer,
        fftSize,
        overlap,
        hop
    };

    CallbackData<T>* deviceData = nullptr;
    const auto allocStatus = cudaMalloc(&deviceData, sizeof(CallbackData<T>));
    if (allocStatus != cudaSuccess) {
        setError(std::string("setupOverlapLoadCallback: cudaMalloc failed: ") +
                 cudaGetErrorString(allocStatus));
        return false;
    }

    const auto copyStatus = cudaMemcpy(
        deviceData,
        &hostData,
        sizeof(CallbackData<T>),
        cudaMemcpyHostToDevice
    );
    if (copyStatus != cudaSuccess) {
        setError(std::string("setupOverlapLoadCallback: cudaMemcpy failed: ") +
                 cudaGetErrorString(copyStatus));
        cudaFree(deviceData);
        return false;
    }

    *callbackData = deviceData;
    return true;
}

bool setupOverlapLoadCallback(cufftHandle plan,
                              float* signal,
                              float* buffer,
                              int fftSize,
                              int overlap,
                              int hop,
                              void** callbackData,
                              std::string* errorMessage)
{
    if (!allocateAndCopyCallbackData(
        signal,
        buffer,
        fftSize,
        overlap,
        hop,
        callbackData,
        errorMessage)) {
        return false;
    }

    auto setError = [&](const std::string& message) {
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        std::cerr << message << std::endl;
    };

    cufftCallbackLoadR loadCallback = nullptr;
    const auto symbolStatus = cudaMemcpyFromSymbol(
        &loadCallback,
        overlap_load_cb_real_ptr,
        sizeof(loadCallback)
    );
    if (symbolStatus != cudaSuccess) {
        setError(std::string("setupOverlapLoadCallback: cudaMemcpyFromSymbol failed: ") +
                 cudaGetErrorString(symbolStatus));
        cudaFree(*callbackData);
        *callbackData = nullptr;
        return false;
    }

    const auto callbackStatus = cufftXtSetCallback(
        plan,
        reinterpret_cast<void**>(&loadCallback),
        CUFFT_CB_LD_REAL,
        reinterpret_cast<void**>(callbackData)
    );
    if (callbackStatus != CUFFT_SUCCESS) {
        setError("setupOverlapLoadCallback: cufftXtSetCallback failed with code " +
                 std::to_string(static_cast<int>(callbackStatus)));
        cudaFree(*callbackData);
        *callbackData = nullptr;
        return false;
    }

    return true;
}

bool setupOverlapLoadCallback(cufftHandle plan,
                              cufftComplex* signal,
                              cufftComplex* buffer,
                              int fftSize,
                              int overlap,
                              int hop,
                              void** callbackData,
                              std::string* errorMessage)
{
    if (!allocateAndCopyCallbackData(
        signal,
        buffer,
        fftSize,
        overlap,
        hop,
        callbackData,
        errorMessage)) {
        return false;
    }

    auto setError = [&](const std::string& message) {
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        std::cerr << message << std::endl;
    };

    cufftCallbackLoadC loadCallback = nullptr;
    const auto symbolStatus = cudaMemcpyFromSymbol(
        &loadCallback,
        overlap_load_cb_ptr,
        sizeof(loadCallback)
    );
    if (symbolStatus != cudaSuccess) {
        setError(std::string("setupOverlapLoadCallback: cudaMemcpyFromSymbol failed: ") +
                 cudaGetErrorString(symbolStatus));
        cudaFree(*callbackData);
        *callbackData = nullptr;
        return false;
    }

    const auto callbackStatus = cufftXtSetCallback(
        plan,
        reinterpret_cast<void**>(&loadCallback),
        CUFFT_CB_LD_COMPLEX,
        reinterpret_cast<void**>(callbackData)
    );
    if (callbackStatus != CUFFT_SUCCESS) {
        setError("setupOverlapLoadCallback: cufftXtSetCallback failed with code " +
                 std::to_string(static_cast<int>(callbackStatus)));
        cudaFree(*callbackData);
        *callbackData = nullptr;
        return false;
    }

    return true;
}

void releaseOverlapLoadCallback(void* callbackData)
{
    if (callbackData != nullptr) {
        cudaFree(callbackData);
    }
}
