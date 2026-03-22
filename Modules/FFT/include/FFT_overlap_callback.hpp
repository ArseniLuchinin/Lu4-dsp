#ifndef FFT_OVERLAP_CALLBACK_HPP
#define FFT_OVERLAP_CALLBACK_HPP

#include <cufft.h>

#include <string>

bool setupOverlapLoadCallback(cufftHandle plan,
                              float* signal,
                              float* buffer,
                              int fftSize,
                              int overlap,
                              int hop,
                              void** callbackData,
                              std::string* errorMessage = nullptr);

bool setupOverlapLoadCallback(cufftHandle plan,
                              cufftComplex* signal,
                              cufftComplex* buffer,
                              int fftSize,
                              int overlap,
                              int hop,
                              void** callbackData,
                              std::string* errorMessage = nullptr);

void releaseOverlapLoadCallback(void* callbackData);

#endif
