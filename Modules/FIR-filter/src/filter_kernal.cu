#include <cuda_runtime.h>
#include <cuComplex.h>
#include <cmath>
#include <chrono>

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

__global__ void energy_float_kernel(const float* in, int N, double* outEnergy)
{
    const int gid = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (gid >= N) {
        return;
    }

    const double v = static_cast<double>(in[gid]);
    atomicAdd(outEnergy, v * v);
}

__global__ void energy_complex_kernel(const cuComplex* in, int N, double* outEnergy)
{
    const int gid = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (gid >= N) {
        return;
    }

    const double re = static_cast<double>(in[gid].x);
    const double im = static_cast<double>(in[gid].y);
    atomicAdd(outEnergy, (re * re) + (im * im));
}

template <typename TSignal>
std::shared_ptr<TSignal> castGpuSignal(
    const std::shared_ptr<IGpuSignalData>& signal,
    const char* /*context*/)
{
    return std::dynamic_pointer_cast<TSignal>(signal);
}

bool ensureEnergyBuffer(double*& buffer)
{
    if (buffer) {
        return true;
    }

    const auto allocErr = cudaMalloc(reinterpret_cast<void**>(&buffer), sizeof(double));
    if (allocErr != cudaSuccess) {
        return false;
    }
    return true;
}

bool computeSignalEnergy(
    const std::shared_ptr<IGpuSignalData>& signal,
    double*& energyBuffer,
    double* outEnergy)
{
    if (!signal || !outEnergy) {
        return false;
    }

    const int N = static_cast<int>(signal->size());
    if (N <= 0) {
        *outEnergy = 0.0;
        return true;
    }

    if (!ensureEnergyBuffer(energyBuffer)) {
        return false;
    }

    const auto clearErr = cudaMemset(energyBuffer, 0, sizeof(double));
    if (clearErr != cudaSuccess) {
        return false;
    }

    const int threads = 256;
    const int blocks = (N + threads - 1) / threads;

    if (signal->sampleType() == GpuSampleType::Float32) {
        auto in = std::dynamic_pointer_cast<GpuFloatSignal>(signal);
        if (!in) {
            return false;
        }
        energy_float_kernel<<<blocks, threads>>>(in->getDeviceData(), N, energyBuffer);
    } else if (signal->sampleType() == GpuSampleType::ComplexFloat32) {
        auto in = std::dynamic_pointer_cast<GpuComplexFloatSignal>(signal);
        if (!in) {
            return false;
        }
        energy_complex_kernel<<<blocks, threads>>>(in->getDeviceData(), N, energyBuffer);
    } else {
        return false;
    }

    const auto launchErr = cudaGetLastError();
    if (launchErr != cudaSuccess) {
        return false;
    }

    const auto copyErr = cudaMemcpy(outEnergy, energyBuffer, sizeof(double), cudaMemcpyDeviceToHost);
    if (copyErr != cudaSuccess) {
        return false;
    }

    return true;
}

} // namespace

bool FIRFilter::run(){
    const auto tRunStart = std::chrono::steady_clock::now();
    if (!m_data || !m_gpuData || !m_workData) {
        ERROR << "FIRFilter::run failed: input/work data is not set." << std::endl;
        return false;
    }

    const auto threads = 256;
    const auto blocks  = static_cast<int>((m_gpuData->size() + threads - 1) / threads);
    DEBUG << "FIRFilter::run begin: inputSize=" << m_gpuData->size()
          << ", blocks=" << blocks
          << ", threads=" << threads
          << ", filterOrder=" << m_M
          << ", logEnergy=" << (m_logEnergy ? "true" : "false") << std::endl;
    if (blocks <= 0) {
        return true;
    }

    double energyBefore = 0.0;
    if (m_logEnergy && !computeSignalEnergy(m_gpuData, m_energyBuffer, &energyBefore)) {
        ERROR << "FIRFilter::run failed: unable to compute input signal energy." << std::endl;
        return false;
    }

    // Fast-path for identity FIR (single real tap == 1): keep input unchanged.
    if (m_tapType == TapType::Real && m_M == 1 && m_realTaps && m_realTaps->isValid()) {
        float tap = 0.0f;
        const auto tapCopyErr = cudaMemcpy(&tap, m_realTaps->getDeviceData(), sizeof(float), cudaMemcpyDeviceToHost);
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

    cudaEvent_t kernelStart = nullptr;
    cudaEvent_t kernelStop = nullptr;
    const auto eventStartErr = cudaEventCreate(&kernelStart);
    const auto eventStopErr = cudaEventCreate(&kernelStop);
    if (eventStartErr != cudaSuccess || eventStopErr != cudaSuccess) {
        if (kernelStart) {
            cudaEventDestroy(kernelStart);
        }
        if (kernelStop) {
            cudaEventDestroy(kernelStop);
        }
        ERROR << "FIRFilter::run failed: cudaEventCreate failed: "
              << cudaGetErrorString((eventStartErr != cudaSuccess) ? eventStartErr : eventStopErr) << std::endl;
        return false;
    }

    const auto tKernelEnqueueStart = std::chrono::steady_clock::now();
    const auto recordKernelStartErr = cudaEventRecord(kernelStart, 0);
    if (recordKernelStartErr != cudaSuccess) {
        cudaEventDestroy(kernelStart);
        cudaEventDestroy(kernelStop);
        ERROR << "FIRFilter::run failed: cudaEventRecord(start) failed: "
              << cudaGetErrorString(recordKernelStartErr) << std::endl;
        return false;
    }

    if (m_tapType == TapType::Real && m_gpuData->sampleType() == GpuSampleType::Float32) {
        auto in = castGpuSignal<GpuFloatSignal>(m_gpuData, "FIRFilter::run");
        auto out = castGpuSignal<GpuFloatSignal>(m_workData, "FIRFilter::run");
        auto history = hasHistory
            ? castGpuSignal<GpuFloatSignal>(m_historyData, "FIRFilter::run")
            : nullptr;
        if (!in || !out || (hasHistory && !history) || !m_realTaps) {
            ERROR << "FIRFilter::run failed: invalid buffers for real input + real taps mode." << std::endl;
            return false;
        }

        fir_global_real_real_kernel<<<blocks, threads>>>(
            in->getDeviceData(),
            out->getDeviceData(),
            static_cast<int>(in->size()),
            hasHistory ? history->getDeviceData() : nullptr,
            m_realTaps->getDeviceData(),
            m_M);
    } else if (m_tapType == TapType::Real && m_gpuData->sampleType() == GpuSampleType::ComplexFloat32) {
        auto in = castGpuSignal<GpuComplexFloatSignal>(m_gpuData, "FIRFilter::run");
        auto out = castGpuSignal<GpuComplexFloatSignal>(m_workData, "FIRFilter::run");
        auto history = hasHistory
            ? castGpuSignal<GpuComplexFloatSignal>(m_historyData, "FIRFilter::run")
            : nullptr;
        if (!in || !out || (hasHistory && !history) || !m_realTaps) {
            ERROR << "FIRFilter::run failed: invalid buffers for complex input + real taps mode." << std::endl;
            return false;
        }

        fir_global_complex_real_kernel<<<blocks, threads>>>(
            in->getDeviceData(),
            out->getDeviceData(),
            static_cast<int>(in->size()),
            hasHistory ? history->getDeviceData() : nullptr,
            m_realTaps->getDeviceData(),
            m_M);
    } else if (m_tapType == TapType::Complex && m_gpuData->sampleType() == GpuSampleType::ComplexFloat32) {
        auto in = castGpuSignal<GpuComplexFloatSignal>(m_gpuData, "FIRFilter::run");
        auto out = castGpuSignal<GpuComplexFloatSignal>(m_workData, "FIRFilter::run");
        auto history = hasHistory
            ? castGpuSignal<GpuComplexFloatSignal>(m_historyData, "FIRFilter::run")
            : nullptr;
        if (!in || !out || (hasHistory && !history) || !m_complexTaps) {
            ERROR << "FIRFilter::run failed: invalid buffers for complex input + complex taps mode." << std::endl;
            return false;
        }

        fir_global_complex_complex_kernel<<<blocks, threads>>>(
            in->getDeviceData(),
            out->getDeviceData(),
            static_cast<int>(in->size()),
            hasHistory ? history->getDeviceData() : nullptr,
            m_complexTaps->getDeviceData(),
            m_M);
    } else {
        cudaEventDestroy(kernelStart);
        cudaEventDestroy(kernelStop);
        ERROR << "FIRFilter::run failed: unsupported input/taps combination." << std::endl;
        return false;
    }

    const auto recordKernelStopErr = cudaEventRecord(kernelStop, 0);
    if (recordKernelStopErr != cudaSuccess) {
        cudaEventDestroy(kernelStart);
        cudaEventDestroy(kernelStop);
        ERROR << "FIRFilter::run failed: cudaEventRecord(stop) failed: "
              << cudaGetErrorString(recordKernelStopErr) << std::endl;
        return false;
    }
    const auto tKernelEnqueueEnd = std::chrono::steady_clock::now();

    const auto launchErr = cudaGetLastError();
    if (launchErr != cudaSuccess) {
        cudaEventDestroy(kernelStart);
        cudaEventDestroy(kernelStop);
        ERROR << "FIRFilter::run failed: kernel launch error: "
              << cudaGetErrorString(launchErr) << std::endl;
        return false;
    }

    const auto tKernelWaitStart = std::chrono::steady_clock::now();
    const auto kernelSyncErr = cudaEventSynchronize(kernelStop);
    const auto tKernelWaitEnd = std::chrono::steady_clock::now();
    if (kernelSyncErr != cudaSuccess) {
        cudaEventDestroy(kernelStart);
        cudaEventDestroy(kernelStop);
        ERROR << "FIRFilter::run failed: cudaEventSynchronize failed: "
              << cudaGetErrorString(kernelSyncErr) << std::endl;
        return false;
    }

    float kernelMs = 0.0f;
    const auto elapsedErr = cudaEventElapsedTime(&kernelMs, kernelStart, kernelStop);
    cudaEventDestroy(kernelStart);
    cudaEventDestroy(kernelStop);
    if (elapsedErr != cudaSuccess) {
        ERROR << "FIRFilter::run failed: cudaEventElapsedTime failed: "
              << cudaGetErrorString(elapsedErr) << std::endl;
        return false;
    }

    const auto kernelEnqueueUs = std::chrono::duration_cast<std::chrono::microseconds>(tKernelEnqueueEnd - tKernelEnqueueStart).count();
    const auto kernelWaitUs = std::chrono::duration_cast<std::chrono::microseconds>(tKernelWaitEnd - tKernelWaitStart).count();
    DEBUG << "FIRFilter::run kernel timing: host_enqueue_us=" << kernelEnqueueUs
          << ", host_wait_us=" << kernelWaitUs
          << ", device_kernel_ms=" << kernelMs << std::endl;

    const auto tCopyBackStart = std::chrono::steady_clock::now();
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
    const auto tCopyBackEnd = std::chrono::steady_clock::now();
    const auto copyBackUs = std::chrono::duration_cast<std::chrono::microseconds>(tCopyBackEnd - tCopyBackStart).count();
    DEBUG << "FIRFilter::run copyBack D2D elapsed_us=" << copyBackUs << std::endl;

    const size_t historySize = static_cast<size_t>(m_M - 1);
    if (historySize == 0) {
        return true;
    }

    if (!m_historyData || !m_nextHistoryData) {
        ERROR << "FIRFilter::run failed: history buffers are not initialized." << std::endl;
        return false;
    }

    const auto tHistoryCopyStart = std::chrono::steady_clock::now();
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
    const auto tHistoryCopyEnd = std::chrono::steady_clock::now();
    const auto historyCopyUs = std::chrono::duration_cast<std::chrono::microseconds>(tHistoryCopyEnd - tHistoryCopyStart).count();
    DEBUG << "FIRFilter::run history copy D2D elapsed_us=" << historyCopyUs << std::endl;

    if (m_logEnergy) {
        double energyAfter = 0.0;
        if (!computeSignalEnergy(m_gpuData, m_energyBuffer, &energyAfter)) {
            ERROR << "FIRFilter::run failed: unable to compute output signal energy." << std::endl;
            return false;
        }

        const double eps = 1.0e-30;
        const double ratio = (energyAfter + eps) / (energyBefore + eps);
        const double ratioDb = 10.0 * std::log10(ratio);
        DEBUG << "FIR energy before=" << energyBefore
              << ", after=" << energyAfter
              << ", delta=" << ratioDb << " dB" << std::endl;
    }

    const auto tRunEnd = std::chrono::steady_clock::now();
    const auto totalUs = std::chrono::duration_cast<std::chrono::microseconds>(tRunEnd - tRunStart).count();
    DEBUG << "FIRFilter::run done: elapsed_total_us=" << totalUs << std::endl;

    return true;
}
