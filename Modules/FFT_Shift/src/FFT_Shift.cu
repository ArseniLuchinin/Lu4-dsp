#include <FFT_Shift.hpp>
#include <VariablesResolve.hpp>

#include <cuda_runtime.h>

#include <module.hpp>

namespace {

__global__ void fftShiftInplaceKernel(float* data, size_t binsPerRow, size_t halfBins, size_t rows)
{
    const size_t pairIdx = (blockIdx.x * blockDim.x) + threadIdx.x;
    const size_t totalPairs = rows * halfBins;
    if (pairIdx >= totalPairs) {
        return;
    }

    const size_t row = pairIdx / halfBins;
    const size_t col = pairIdx % halfBins;
    const size_t leftIdx = row * binsPerRow + col;
    const size_t rightIdx = leftIdx + halfBins;

    const float tmp = data[leftIdx];
    data[leftIdx] = data[rightIdx];
    data[rightIdx] = tmp;
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
    : IModule({"FFT_Shift", "FFT_Shift-module.so", "FFT_Shift.json"}) {}

bool FFT_Shift::init() {
    if (m_fftSize == 0) {
        ERROR << "FFT_Shift::init: fft size must be greater than zero." << std::endl;
        return false;
    }

    if (m_windowSize > 0) {
        const size_t realBins = (m_fftSize / 2) + 1;
        const size_t complexBins = m_fftSize;
        if (m_windowSize != realBins && m_windowSize != complexBins) {
            ERROR << "FFT_Shift::init: window size must be " << realBins
                  << " (real spectrum) or " << complexBins << " (complex spectrum)." << std::endl;
            return false;
        }
    }

    return true;
}

bool FFT_Shift::run() {
    float* dataPtr = m_inData->getDeviceData();

    constexpr int blockSize = 256;
    const int gridSize = static_cast<int>((m_totalPairs + blockSize - 1) / blockSize);
    fftShiftInplaceKernel<<<gridSize, blockSize>>>(dataPtr, m_binsPerRow, m_halfBins, m_rows);

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

    const size_t totalSize = m_inData->size();
    if (totalSize == 0) {
        ERROR << "FFT_Shift::setData: input data size is zero." << std::endl;
        return false;
    }

    size_t binsPerRow = 0;
    bool isRealSpectrum = false;
    if (!resolveInputLayout(totalSize, &binsPerRow, &isRealSpectrum)) {
        ERROR << "FFT_Shift::setData: failed to resolve spectrum width for input size " << totalSize << "."
              << std::endl;
        return false;
    }

    if (isRealSpectrum) {
        ERROR << "FFT_Shift::setData: fft shift is supported only for full complex spectrum." << std::endl;
        return false;
    }

    if ((totalSize % binsPerRow) != 0) {
        ERROR << "FFT_Shift::setData: input size (" << totalSize
              << ") is not divisible by row width (" << binsPerRow << ")." << std::endl;
        return false;
    }

    if ((binsPerRow % 2) != 0) {
        ERROR << "FFT_Shift::setData: inplace fft shift requires even row width." << std::endl;
        return false;
    }

    m_binsPerRow = binsPerRow;
    m_rows = totalSize / binsPerRow;
    m_halfBins = binsPerRow / 2;
    m_totalPairs = m_rows * m_halfBins;
    return true;
}

std::shared_ptr<IData> FFT_Shift::getData() {
    return m_inData;
}

bool FFT_Shift::resolveInputLayout(size_t totalSize, size_t* freqBins, bool* isRealSpectrum) const
{
    return resolveFreqBins(m_fftSize, m_windowSize, totalSize, freqBins, isRealSpectrum);
}
