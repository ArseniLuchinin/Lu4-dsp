#include <FFT_Shift.hpp>
#include <VariablesResolve.hpp>

#include <cuda_runtime.h>

#include <module.hpp>

namespace {

__global__ void fftShiftKernel(const float* in, float* out, size_t binsPerRow, size_t totalSize)
{
    const size_t idx = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (idx >= totalSize) {
        return;
    }

    const size_t col = idx % binsPerRow;
    const size_t rowStart = idx - col;
    const size_t shift = binsPerRow / 2;
    const size_t srcCol = (col + shift) % binsPerRow;

    out[idx] = in[rowStart + srcCol];
}

bool resolveFreqBins(size_t fftSize,
                     size_t windowSize,
                     size_t totalSize,
                     size_t* freqBins,
                     bool* isRealSpectrum)
{
    if (freqBins == nullptr || isRealSpectrum == nullptr) {
        return false;
    }

    const size_t realBins = (fftSize / 2) + 1;
    const size_t complexBins = fftSize;
    if (realBins == 0 || complexBins == 0) {
        return false;
    }

    if (windowSize > 0) {
        if (windowSize != realBins && windowSize != complexBins) {
            return false;
        }
        *freqBins = windowSize;
        *isRealSpectrum = (windowSize == realBins);
        return true;
    }

    const bool divisibleReal = (totalSize >= realBins) && ((totalSize % realBins) == 0);
    const bool divisibleComplex = (totalSize >= complexBins) && ((totalSize % complexBins) == 0);

    if (divisibleReal) {
        *freqBins = realBins;
        *isRealSpectrum = true;
        return true;
    }
    if (divisibleComplex) {
        *freqBins = complexBins;
        *isRealSpectrum = false;
        return true;
    }

    return false;
}

} // namespace

IModule* createModule() {
    return new FFT_Shift();
}

FFT_Shift::FFT_Shift()
    : IModule({"FFT Shift", "FFT_Shift-module.so", "FFT_Shift.json"}) {}

bool FFT_Shift::init() {
    return true;
}

bool FFT_Shift::run() {
    if (!m_inData || !m_inData->isValid()) {
        ERROR << "FFT_Shift::run: input data is null or invalid." << std::endl;
        return false;
    }

    const size_t totalSize = m_inData->size();
    if (totalSize == 0) {
        ERROR << "FFT_Shift::run: input data size is zero." << std::endl;
        return false;
    }

    if (m_fftSize == 0) {
        ERROR << "FFT_Shift::run: fft size must be greater than zero." << std::endl;
        return false;
    }

    size_t binsPerRow = 0;
    bool isRealSpectrum = false;
    if (!resolveFreqBins(m_fftSize, m_windowSize, totalSize, &binsPerRow, &isRealSpectrum)) {
        ERROR << "FFT_Shift::run: failed to resolve spectrum width. "
              << "Expected window size " << ((m_fftSize / 2) + 1)
              << " (real) or " << m_fftSize << " (complex)." << std::endl;
        return false;
    }

    if (isRealSpectrum) {
        ERROR << "FFT_Shift::run: fft shift is supported only for full complex spectrum." << std::endl;
        return false;
    }

    if ((totalSize % binsPerRow) != 0) {
        ERROR << "FFT_Shift::run: input size (" << totalSize
              << ") is not divisible by row width (" << binsPerRow << ")." << std::endl;
        return false;
    }

    if (!m_outData || m_outData->availableSize() < totalSize) {
        m_outData = std::make_shared<GpuFloatSignal>(totalSize);
    }
    if (!m_outData || !m_outData->isValid()) {
        ERROR << "FFT_Shift::run: failed to allocate output buffer." << std::endl;
        return false;
    }
    if (!m_outData->setLogicalSize(totalSize)) {
        ERROR << "FFT_Shift::run: failed to set output logical size." << std::endl;
        return false;
    }

    const float* inPtr = m_inData->getDeviceData();
    float* outPtr = m_outData->getDeviceData();
    if (!inPtr || !outPtr) {
        ERROR << "FFT_Shift::run: null device pointer(s)." << std::endl;
        return false;
    }

    constexpr int blockSize = 256;
    const int gridSize = static_cast<int>((totalSize + blockSize - 1) / blockSize);
    fftShiftKernel<<<gridSize, blockSize>>>(inPtr, outPtr, binsPerRow, totalSize);

    const auto launchErr = cudaGetLastError();
    if (launchErr != cudaSuccess) {
        ERROR << "FFT_Shift::run: kernel launch failed: " << cudaGetErrorString(launchErr) << std::endl;
        return false;
    }

    const auto syncErr = cudaDeviceSynchronize();
    if (syncErr != cudaSuccess) {
        ERROR << "FFT_Shift::run: kernel execution failed: " << cudaGetErrorString(syncErr) << std::endl;
        return false;
    }

    return true;
}

void FFT_Shift::setParam(const std::string& paramName, const std::any& value) {
    const std::any resolved = resolveParamValue(value);
    if (paramName == "fft size") {
        m_fftSize = static_cast<size_t>(std::any_cast<int32_t>(resolved));
        return;
    }

    if (paramName == "window size") {
        m_windowSize = static_cast<size_t>(std::any_cast<int32_t>(resolved));
        return;
    }
}

bool FFT_Shift::setData(std::shared_ptr<IData> data) {
    m_inData = std::dynamic_pointer_cast<GpuFloatSignal>(data);
    if (!m_inData) {
        ERROR << "FFT_Shift::setData: input data is not a valid GpuFloatSignal." << std::endl;
        return false;
    }
    return true;
}

std::shared_ptr<IData> FFT_Shift::getData() {
    return m_outData;
}
