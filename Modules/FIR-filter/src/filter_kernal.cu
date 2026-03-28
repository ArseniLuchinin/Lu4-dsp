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

bool computeSignalEnergy(const std::shared_ptr<IGpuSignalData>& signal, double* outEnergy)
{
    if (!signal || !outEnergy) {
        return false;
    }

    const int N = static_cast<int>(signal->size());
    if (N <= 0) {
        *outEnergy = 0.0;
        return true;
    }

    double* dEnergy = nullptr;
    const auto allocErr = cudaMalloc(&dEnergy, sizeof(double));
    if (allocErr != cudaSuccess) {
        return false;
    }

    const auto clearErr = cudaMemset(dEnergy, 0, sizeof(double));
    if (clearErr != cudaSuccess) {
        cudaFree(dEnergy);
        return false;
    }

    const int threads = 256;
    const int blocks = (N + threads - 1) / threads;

    if (signal->sampleType() == GpuSampleType::Float32) {
        auto in = std::dynamic_pointer_cast<GpuFloatSignal>(signal);
        if (!in) {
            cudaFree(dEnergy);
            return false;
        }
        energy_float_kernel<<<blocks, threads>>>(in->getDeviceData(), N, dEnergy);
    } else if (signal->sampleType() == GpuSampleType::ComplexFloat32) {
        auto in = std::dynamic_pointer_cast<GpuComplexFloatSignal>(signal);
        if (!in) {
            cudaFree(dEnergy);
            return false;
        }
        energy_complex_kernel<<<blocks, threads>>>(in->getDeviceData(), N, dEnergy);
    } else {
        cudaFree(dEnergy);
        return false;
    }

    const auto launchErr = cudaGetLastError();
    if (launchErr != cudaSuccess) {
        cudaFree(dEnergy);
        return false;
    }

    const auto syncErr = cudaDeviceSynchronize();
    if (syncErr != cudaSuccess) {
        cudaFree(dEnergy);
        return false;
    }

    const auto copyErr = cudaMemcpy(outEnergy, dEnergy, sizeof(double), cudaMemcpyDeviceToHost);
    cudaFree(dEnergy);
    if (copyErr != cudaSuccess) {
        return false;
    }

    return true;
}

} // namespace

bool FIRFilter::run(){
    if (!m_data || !m_gpuData || !m_workData) {
        ERROR << "FIRFilter::run failed: input/work data is not set." << std::endl;
        return false;
    }

    const auto threads = 256;
    const auto blocks  = static_cast<int>((m_gpuData->size() + threads - 1) / threads);
    if (blocks <= 0) {
        return true;
    }

    double energyBefore = 0.0;
    if (m_logEnergy && !computeSignalEnergy(m_gpuData, &energyBefore)) {
        ERROR << "FIRFilter::run failed: unable to compute input signal energy." << std::endl;
        return false;
    }

    if (m_tapType == TapType::Real && m_gpuData->sampleType() == GpuSampleType::Float32) {
        auto in = std::dynamic_pointer_cast<GpuFloatSignal>(m_gpuData);
        auto out = std::dynamic_pointer_cast<GpuFloatSignal>(m_workData);
        auto history = std::dynamic_pointer_cast<GpuFloatSignal>(m_historyData);
        if (!in || !out || !history || !m_realTaps) {
            ERROR << "FIRFilter::run failed: invalid buffers for real input + real taps mode." << std::endl;
            return false;
        }

        fir_global_real_real_kernel<<<blocks, threads>>>(
            in->getDeviceData(),
            out->getDeviceData(),
            static_cast<int>(in->size()),
            history->getDeviceData(),
            m_realTaps->getDeviceData(),
            m_M);
    } else if (m_tapType == TapType::Real && m_gpuData->sampleType() == GpuSampleType::ComplexFloat32) {
        auto in = std::dynamic_pointer_cast<GpuComplexFloatSignal>(m_gpuData);
        auto out = std::dynamic_pointer_cast<GpuComplexFloatSignal>(m_workData);
        auto history = std::dynamic_pointer_cast<GpuComplexFloatSignal>(m_historyData);
        if (!in || !out || !history || !m_realTaps) {
            ERROR << "FIRFilter::run failed: invalid buffers for complex input + real taps mode." << std::endl;
            return false;
        }

        fir_global_complex_real_kernel<<<blocks, threads>>>(
            in->getDeviceData(),
            out->getDeviceData(),
            static_cast<int>(in->size()),
            history->getDeviceData(),
            m_realTaps->getDeviceData(),
            m_M);
    } else if (m_tapType == TapType::Complex && m_gpuData->sampleType() == GpuSampleType::ComplexFloat32) {
        auto in = std::dynamic_pointer_cast<GpuComplexFloatSignal>(m_gpuData);
        auto out = std::dynamic_pointer_cast<GpuComplexFloatSignal>(m_workData);
        auto history = std::dynamic_pointer_cast<GpuComplexFloatSignal>(m_historyData);
        if (!in || !out || !history || !m_complexTaps) {
            ERROR << "FIRFilter::run failed: invalid buffers for complex input + complex taps mode." << std::endl;
            return false;
        }

        fir_global_complex_complex_kernel<<<blocks, threads>>>(
            in->getDeviceData(),
            out->getDeviceData(),
            static_cast<int>(in->size()),
            history->getDeviceData(),
            m_complexTaps->getDeviceData(),
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

    const auto kernelSyncErr = cudaDeviceSynchronize();
    if (kernelSyncErr != cudaSuccess) {
        ERROR << "FIRFilter::run failed: kernel execution error: "
              << cudaGetErrorString(kernelSyncErr) << std::endl;
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

    const auto copyBackSyncErr = cudaDeviceSynchronize();
    if (copyBackSyncErr != cudaSuccess) {
        ERROR << "FIRFilter::run failed: copy back sync error: "
              << cudaGetErrorString(copyBackSyncErr) << std::endl;
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

    const auto historySyncErr = cudaDeviceSynchronize();
    if (historySyncErr != cudaSuccess) {
        ERROR << "FIRFilter::run failed: history sync error: "
              << cudaGetErrorString(historySyncErr) << std::endl;
        return false;
    }

    if (m_logEnergy) {
        double energyAfter = 0.0;
        if (!computeSignalEnergy(m_gpuData, &energyAfter)) {
            ERROR << "FIRFilter::run failed: unable to compute output signal energy." << std::endl;
            return false;
        }

        const double eps = 1.0e-30;
        const double ratio = (energyAfter + eps) / (energyBefore + eps);
        const double ratioDb = 10.0 * std::log10(ratio);
        INFO << "FIR energy before=" << energyBefore
             << ", after=" << energyAfter
             << ", delta=" << ratioDb << " dB" << std::endl;
    }

    return true;
}
