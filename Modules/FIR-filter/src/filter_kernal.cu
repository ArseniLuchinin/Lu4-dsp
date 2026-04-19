#include <cuda_runtime.h>
#include <cuComplex.h>
#include <cmath>

#include <FIR_filter.hpp>

namespace {

__device__ inline float loadRealSample(const float* in, const float* history, int n, int k, int historySize)
{
    const int idx = n - k;
    if (idx >= 0) {
        return in[idx];
    }
    return history[historySize + idx];
}

__device__ inline cuComplex loadComplexSample(const cuComplex* in, const cuComplex* history, int n, int k, int historySize)
{
    const int idx = n - k;
    if (idx >= 0) {
        return in[idx];
    }
    return history[historySize + idx];
}

__global__ void fir_global_real_real_kernel(
    const float* in,
    float* out,
    int N,
    const float* history,
    const float* taps,
    int M)
{
    const int gid = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (gid >= N) {
        return;
    }

    const int historySize = M - 1;
    float acc = 0.0f;
    for (int k = 0; k < M; ++k) {
        const float sample = loadRealSample(in, history, gid, k, historySize);
        acc += sample * taps[k];
    }

    out[gid] = acc;
}

__global__ void fir_global_complex_real_kernel(
    const cuComplex* in,
    cuComplex* out,
    int N,
    const cuComplex* history,
    const float* taps,
    int M)
{
    const int gid = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (gid >= N) {
        return;
    }

    const int historySize = M - 1;
    float accRe = 0.0f;
    float accIm = 0.0f;
    for (int k = 0; k < M; ++k) {
        const cuComplex sample = loadComplexSample(in, history, gid, k, historySize);
        const float tap = taps[k];
        accRe += sample.x * tap;
        accIm += sample.y * tap;
    }

    out[gid] = make_cuComplex(accRe, accIm);
}

__global__ void fir_global_complex_complex_kernel(
    const cuComplex* in,
    cuComplex* out,
    int N,
    const cuComplex* history,
    const cuComplex* taps,
    int M)
{
    const int gid = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (gid >= N) {
        return;
    }

    const int historySize = M - 1;
    float accRe = 0.0f;
    float accIm = 0.0f;
    for (int k = 0; k < M; ++k) {
        const cuComplex sample = loadComplexSample(in, history, gid, k, historySize);
        const cuComplex tap = taps[k];

        // Complex multiply: sample * tap
        accRe += (sample.x * tap.x) - (sample.y * tap.y);
        accIm += (sample.x * tap.y) + (sample.y * tap.x);
    }

    out[gid] = make_cuComplex(accRe, accIm);
}

template <typename TSignal>
std::shared_ptr<TSignal> castGpuSignal(
    const std::shared_ptr<IGpuSignalData>& signal,
    const char* /*context*/)
{
    return std::dynamic_pointer_cast<TSignal>(signal);
}

} // namespace

bool FIRFilter::run(){
    if (!m_data || !m_gpuData || !m_workData || !m_taps) {
        ERROR << "FIRFilter::run failed: input/work data is not set." << std::endl;
        return false;
    }

    const auto threads = 256;
    const auto blocks  = static_cast<int>((m_gpuData->size() + threads - 1) / threads);
    if (blocks <= 0) {
        return true;
    }

    // Fast-path for identity FIR (single real tap == 1): keep input unchanged.
    if (m_taps->sampleType() == GpuSampleType::Float32 && m_M == 1 && m_taps->isValid()) {
        float tap = 0.0f;
        auto realTaps = castGpuSignal<GpuFloatSignal>(m_taps, "FIRFilter::run");
        if (!realTaps) {
            ERROR << "FIRFilter::run failed: unable to access single real tap value." << std::endl;
            return false;
        }

        const auto tapCopyErr = cudaMemcpy(&tap, realTaps->getDeviceData(), sizeof(float), cudaMemcpyDeviceToHost);
        if (tapCopyErr != cudaSuccess) {
            ERROR << "FIRFilter::run failed: unable to read single tap value: "
                  << cudaGetErrorString(tapCopyErr) << std::endl;
            return false;
        }

        if (std::abs(tap - 1.0f) <= 1.0e-7f) {
            return true;
        }
    }

    const bool hasHistory = (m_M > 1);

    if (m_taps->sampleType() == GpuSampleType::Float32 &&
        m_gpuData->sampleType() == GpuSampleType::Float32) {
        auto in = castGpuSignal<GpuFloatSignal>(m_gpuData, "FIRFilter::run");
        auto out = castGpuSignal<GpuFloatSignal>(m_workData, "FIRFilter::run");
        auto history = hasHistory
            ? castGpuSignal<GpuFloatSignal>(m_historyData, "FIRFilter::run")
            : nullptr;
        auto taps = castGpuSignal<GpuFloatSignal>(m_taps, "FIRFilter::run");
        if (!in || !out || (hasHistory && !history) || !taps) {
            ERROR << "FIRFilter::run failed: invalid buffers for real input + real taps mode." << std::endl;
            return false;
        }

        fir_global_real_real_kernel<<<blocks, threads>>>(
            in->getDeviceData(),
            out->getDeviceData(),
            static_cast<int>(in->size()),
            hasHistory ? history->getDeviceData() : nullptr,
            taps->getDeviceData(),
            m_M);
    } else if (m_taps->sampleType() == GpuSampleType::Float32 &&
               m_gpuData->sampleType() == GpuSampleType::ComplexFloat32) {
        auto in = castGpuSignal<GpuComplexFloatSignal>(m_gpuData, "FIRFilter::run");
        auto out = castGpuSignal<GpuComplexFloatSignal>(m_workData, "FIRFilter::run");
        auto history = hasHistory
            ? castGpuSignal<GpuComplexFloatSignal>(m_historyData, "FIRFilter::run")
            : nullptr;
        auto taps = castGpuSignal<GpuFloatSignal>(m_taps, "FIRFilter::run");
        if (!in || !out || (hasHistory && !history) || !taps) {
            ERROR << "FIRFilter::run failed: invalid buffers for complex input + real taps mode." << std::endl;
            return false;
        }

        fir_global_complex_real_kernel<<<blocks, threads>>>(
            in->getDeviceData(),
            out->getDeviceData(),
            static_cast<int>(in->size()),
            hasHistory ? history->getDeviceData() : nullptr,
            taps->getDeviceData(),
            m_M);
    } else if (m_taps->sampleType() == GpuSampleType::ComplexFloat32 &&
               m_gpuData->sampleType() == GpuSampleType::ComplexFloat32) {
        auto in = castGpuSignal<GpuComplexFloatSignal>(m_gpuData, "FIRFilter::run");
        auto out = castGpuSignal<GpuComplexFloatSignal>(m_workData, "FIRFilter::run");
        auto history = hasHistory
            ? castGpuSignal<GpuComplexFloatSignal>(m_historyData, "FIRFilter::run")
            : nullptr;
        auto taps = castGpuSignal<GpuComplexFloatSignal>(m_taps, "FIRFilter::run");
        if (!in || !out || (hasHistory && !history) || !taps) {
            ERROR << "FIRFilter::run failed: invalid buffers for complex input + complex taps mode." << std::endl;
            return false;
        }

        fir_global_complex_complex_kernel<<<blocks, threads>>>(
            in->getDeviceData(),
            out->getDeviceData(),
            static_cast<int>(in->size()),
            hasHistory ? history->getDeviceData() : nullptr,
            taps->getDeviceData(),
            m_M);
    } else {
        ERROR << "FIRFilter::run failed: unsupported input/taps combination." << std::endl;
        return false;
    }

    const auto launchErr = cudaGetLastError();
    if (launchErr != cudaSuccess) {
        ERROR << "FIRFilter::run failed: kernel launch error: "
              << cudaGetErrorString(launchErr) << std::endl;
        return false;
    }

    const auto syncErr = cudaDeviceSynchronize();
    if (syncErr != cudaSuccess) {
        ERROR << "FIRFilter::run failed: cudaDeviceSynchronize failed: "
              << cudaGetErrorString(syncErr) << std::endl;
        return false;
    }

    const auto copyBackErr = cudaMemcpy(
        m_gpuData->deviceDataRaw(),
        m_workData->deviceDataRaw(),
        m_gpuData->size() * m_gpuData->elementSizeBytes(),
        cudaMemcpyDeviceToDevice
    );
    if (copyBackErr != cudaSuccess) {
        ERROR << "FIRFilter::run failed: copy back error: "
              << cudaGetErrorString(copyBackErr) << std::endl;
        return false;
    }

    const size_t historySize = static_cast<size_t>(m_M - 1);
    if (historySize == 0) {
        return true;
    }

    if (!m_historyData || !m_nextHistoryData) {
        ERROR << "FIRFilter::run failed: history buffers are not initialized." << std::endl;
        return false;
    }

    const auto copyErr = cudaMemcpy(
        m_historyData->deviceDataRaw(),
        m_nextHistoryData->deviceDataRaw(),
        historySize * m_gpuData->elementSizeBytes(),
        cudaMemcpyDeviceToDevice
    );
    if (copyErr != cudaSuccess) {
        ERROR << "FIRFilter::run failed: history update copy error: "
              << cudaGetErrorString(copyErr) << std::endl;
        return false;
    }

    return true;
}
