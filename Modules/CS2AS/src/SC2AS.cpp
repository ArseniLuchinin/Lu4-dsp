#include <CS2AS.hpp>
#include <VariablesResolve.hpp>

#include <cuda_runtime.h>
#include <cuComplex.h>

#include <memory>
#include <module.hpp>

namespace {
__global__ void complexToAmplitudeKernel(const cuComplex* in, float* out, size_t n, float powerNorm) {
    const size_t i = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (i >= n) {
        return;
    }

    const float re = cuCrealf(in[i]);
    const float im = cuCimagf(in[i]);
    const float power = ((re * re) + (im * im)) * powerNorm;
    out[i] = 10.0 * __log10f(fmaxf(power, 1.0e-20f));
}
}

IModule* createModule() {
    return new CS2AS();
}

bool CS2AS::init() {
    return true;
}

bool CS2AS::run() {
    if (!m_inData || !m_inData->isValid()) {
        ERROR << "CS2AS::run: input complex spectrum is null or invalid." << std::endl;
        return false;
    }

    const auto inSize = m_inData->size();
    if (inSize == 0) {
        ERROR << "CS2AS::run: input complex spectrum has zero size." << std::endl;
        return false;
    }

    if (!m_outData || m_outData->availableSize() < inSize) {
        m_outData = std::make_shared<GpuFloatSignal>(inSize);
    }

    if (!m_outData->isValid()) {
        ERROR << "CS2AS::run: output amplitude buffer is invalid." << std::endl;
        return false;
    }

    if (!m_outData->setLogicalSize(inSize)) {
        ERROR << "CS2AS::run: failed to set output logical size." << std::endl;
        return false;
    }

    const auto* inPtr = m_inData->getDeviceData();
    auto* outPtr = m_outData->getDeviceData();
    if (!inPtr || !outPtr) {
        ERROR << "CS2AS::run: null device pointer(s)." << std::endl;
        return false;
    }

    float powerNorm = 1.0f;
    if (m_normalizeByFftSize) {
        const float fftNorm = static_cast<float>(m_fftSize > 0 ? m_fftSize : 1);
        powerNorm = 1.0f / (fftNorm * fftNorm);
    }

    constexpr int blockSize = 256;
    const int gridSize = static_cast<int>((inSize + blockSize - 1) / blockSize);
    complexToAmplitudeKernel<<<gridSize, blockSize>>>(inPtr, outPtr, inSize, powerNorm);

    const auto launchErr = cudaGetLastError();
    if (launchErr != cudaSuccess) {
        ERROR << "CS2AS::run: kernel launch failed: " << cudaGetErrorString(launchErr) << std::endl;
        return false;
    }

    const auto syncErr = cudaDeviceSynchronize();
    if (syncErr != cudaSuccess) {
        ERROR << "CS2AS::run: kernel execution failed: " << cudaGetErrorString(syncErr) << std::endl;
        return false;
    }

    return true;
}

void CS2AS::setParam(const std::string& paramName, const std::any& value) {
    if (paramName == "fft size") {
        m_fftSize = static_cast<size_t>(std::any_cast<int64_t>(value));
        return;
    }

    if (paramName == "normalize by fft size") {
        m_normalizeByFftSize = std::any_cast<bool>(value);
        return;
    }
}

bool CS2AS::setData(std::shared_ptr<IData> data) {
    m_inData = std::dynamic_pointer_cast<GpuComplexFloatSignal>(data);
    return m_inData != nullptr;
}

std::shared_ptr<IData> CS2AS::getData() {
    return m_outData;
}
