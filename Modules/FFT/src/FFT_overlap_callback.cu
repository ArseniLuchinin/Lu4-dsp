#include "FFT_overlap_callback.hpp"

#include <cufftXt.h>
#include <cuda_runtime.h>

#include <iostream>

namespace {

struct CallbackData {
    cufftComplex* signal;
    cufftComplex* buffer;
    int fftSize;
    int overlap;
    int hop;
};

__device__ cufftComplex overlap_load_cb(void* dataIn,
                                        size_t offset,
                                        void* callerInfo,
                                        void* sharedPtr)
{
    (void)dataIn;
    (void)sharedPtr;

    auto* cb = static_cast<CallbackData*>(callerInfo);

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

__device__ cufftCallbackLoadC overlap_load_cb_ptr = overlap_load_cb;

} // namespace

bool setupOverlapLoadCallback(cufftHandle plan,
                              cufftComplex* signal,
                              cufftComplex* buffer,
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

    CallbackData hostData{
        signal,
        buffer,
        fftSize,
        overlap,
        hop
    };

    CallbackData* deviceData = nullptr;
    const auto allocStatus = cudaMalloc(&deviceData, sizeof(CallbackData));
    if (allocStatus != cudaSuccess) {
        setError(std::string("setupOverlapLoadCallback: cudaMalloc failed: ") +
                 cudaGetErrorString(allocStatus));
        return false;
    }

    const auto copyStatus = cudaMemcpy(
        deviceData,
        &hostData,
        sizeof(CallbackData),
        cudaMemcpyHostToDevice
    );
    if (copyStatus != cudaSuccess) {
        setError(std::string("setupOverlapLoadCallback: cudaMemcpy failed: ") +
                 cudaGetErrorString(copyStatus));
        cudaFree(deviceData);
        return false;
    }

    cufftCallbackLoadC loadCallback = nullptr;
    const auto symbolStatus = cudaMemcpyFromSymbol(
        &loadCallback,
        overlap_load_cb_ptr,
        sizeof(loadCallback)
    );
    if (symbolStatus != cudaSuccess) {
        setError(std::string("setupOverlapLoadCallback: cudaMemcpyFromSymbol failed: ") +
                 cudaGetErrorString(symbolStatus));
        cudaFree(deviceData);
        return false;
    }

    const auto callbackStatus = cufftXtSetCallback(
        plan,
        reinterpret_cast<void**>(&loadCallback),
        CUFFT_CB_LD_COMPLEX,
        reinterpret_cast<void**>(&deviceData)
    );
    if (callbackStatus != CUFFT_SUCCESS) {
        setError("setupOverlapLoadCallback: cufftXtSetCallback failed with code " +
                 std::to_string(static_cast<int>(callbackStatus)));
        cudaFree(deviceData);
        return false;
    }

    *callbackData = deviceData;
    return true;
}

void releaseOverlapLoadCallback(void* callbackData)
{
    if (callbackData != nullptr) {
        cudaFree(callbackData);
    }
}
