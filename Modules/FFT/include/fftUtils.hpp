#ifndef FFT_UTILS_HPP
#define FFT_UTILS_HPP

#include <cuda_runtime.h>
#include <cufft.h>

#include <iostream>
#include <memory>

inline const char* cufftResultToString(cufftResult result) {
    switch (result) {
    case CUFFT_SUCCESS:
        return "CUFFT_SUCCESS";
    case CUFFT_INVALID_PLAN:
        return "CUFFT_INVALID_PLAN";
    case CUFFT_ALLOC_FAILED:
        return "CUFFT_ALLOC_FAILED";
    case CUFFT_INVALID_TYPE:
        return "CUFFT_INVALID_TYPE";
    case CUFFT_INVALID_VALUE:
        return "CUFFT_INVALID_VALUE";
    case CUFFT_INTERNAL_ERROR:
        return "CUFFT_INTERNAL_ERROR";
    case CUFFT_EXEC_FAILED:
        return "CUFFT_EXEC_FAILED";
    case CUFFT_SETUP_FAILED:
        return "CUFFT_SETUP_FAILED";
    case CUFFT_INVALID_SIZE:
        return "CUFFT_INVALID_SIZE";
    case CUFFT_UNALIGNED_DATA:
        return "CUFFT_UNALIGNED_DATA";
    default:
        return "CUFFT_UNKNOWN_ERROR";
    }
}

template <typename SignalT>
bool saveInputTailToBufferImpl(
    SignalT& buffer,
    const std::shared_ptr<SignalT>& inData,
    size_t overlap
) {
    if (!buffer.reserve(overlap)) {
        std::cerr << "FFT::saveInputTailToBuffer: failed to reserve buffer." << std::endl;
        return false;
    }

    auto* inPtr = inData->getDeviceData();
    buffer.setDataFromDevice(inPtr + (inData->size() - overlap), overlap);
    return true;
}

template <typename SignalT>
bool preparePrefixInputImpl(
    SignalT& prefixInput,
    SignalT& buffer,
    const std::shared_ptr<SignalT>& inData,
    size_t fftSize,
    size_t overlap
) {
    if (!prefixInput.reserve(fftSize)) {
        std::cerr << "FFT::executeStitchFft: failed to reserve prefix input." << std::endl;
        return false;
    }

    const size_t headSize = fftSize - overlap;
    auto* prefixPtr = prefixInput.getDeviceData();
    auto* bufferPtr = buffer.getDeviceData();
    auto* inPtr = inData->getDeviceData();

    const auto copyTail = cudaMemcpy(
        prefixPtr,
        bufferPtr,
        overlap * sizeof(*prefixPtr),
        cudaMemcpyDeviceToDevice
    );
    if (copyTail != cudaSuccess) {
        std::cerr << "FFT::executeStitchFft: tail copy failed: " << cudaGetErrorString(copyTail) << std::endl;
        return false;
    }

    const auto copyHead = cudaMemcpy(
        prefixPtr + overlap,
        inPtr,
        headSize * sizeof(*prefixPtr),
        cudaMemcpyDeviceToDevice
    );
    if (copyHead != cudaSuccess) {
        std::cerr << "FFT::executeStitchFft: head copy failed: " << cudaGetErrorString(copyHead) << std::endl;
        return false;
    }

    return true;
}

#endif
