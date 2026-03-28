#include <cuda_runtime.h>
#include <cuComplex.h>
#include <vector>
#include <cmath>
#include <stdexcept>
#include <FIR_filter.hpp>

#define MAX_FIR_LENGTH 2048

__constant__ float d_h[MAX_FIR_LENGTH];

__global__ void fir_inplace_kernel(
    float* data,   // вход и выход один и тот же массив
    int N,
    float* history, // При первом расчёта заполнена нулями
    int M)
{
    extern __shared__ float s_data[];

    int tid  = threadIdx.x;
    int gid  = blockIdx.x * blockDim.x + tid;

    int halo = M - 1;

    // указатель на блок входных данных
    int base = blockIdx.x * blockDim.x;

    // загрузка основной части
    if (gid < N)
        s_data[tid + halo] = data[gid];

    // загрузка "истории" (halo)
    // TODO здесь будут браться данные из буфер
    // PREFIX_BUFFER[MAX_FIR_LENGTH]. В первой итериции все нули,
    // в последующийх прошлые M-1 значений 
    if (tid < halo)
    {
        int idx = base + tid - halo;
        s_data[tid] = idx >= 0 ?
            data[idx] :
            history[halo + idx];
    }

    __syncthreads();

    if (gid >= N)
        return;

    float acc = 0.0f;

    int half = M >> 1;

    #pragma unroll 4
    for (int k = 0; k < half; k++)
    {
        float x1 = s_data[tid + k];
        float x2 = s_data[tid + M - 1 - k];

        acc += d_h[k] * (x1 + x2);
    }

    acc += d_h[half] * s_data[tid + half];

    // запись результата обратно (in-place)
    data[gid] = acc;
}

__global__ void fir_inplace_complex_kernel(
    cuComplex* data,
    int N,
    const cuComplex* history,
    int M)
{
    extern __shared__ cuComplex s_data_complex[];

    const int tid = threadIdx.x;
    const int gid = blockIdx.x * blockDim.x + tid;
    const int halo = M - 1;
    const int base = blockIdx.x * blockDim.x;

    if (gid < N) {
        s_data_complex[tid + halo] = data[gid];
    }

    if (tid < halo) {
        const int idx = base + tid - halo;
        s_data_complex[tid] = (idx >= 0) ? data[idx] : history[halo + idx];
    }

    __syncthreads();

    if (gid >= N) {
        return;
    }

    float accRe = 0.0f;
    float accIm = 0.0f;
    const int half = M >> 1;

    #pragma unroll 4
    for (int k = 0; k < half; ++k) {
        const cuComplex x1 = s_data_complex[tid + k];
        const cuComplex x2 = s_data_complex[tid + M - 1 - k];
        const float sumRe = x1.x + x2.x;
        const float sumIm = x1.y + x2.y;

        accRe += d_h[k] * sumRe;
        accIm += d_h[k] * sumIm;
    }

    const cuComplex xc = s_data_complex[tid + half];
    accRe += d_h[half] * xc.x;
    accIm += d_h[half] * xc.y;

    data[gid] = make_cuComplex(accRe, accIm);
}

bool FIRFilter::run(){
    if (!m_data || !m_gpuData) {
        ERROR << "FIRFilter::run failed: input data is not set." << std::endl;
        return false;
    }

    const auto threads = 256;
    const auto blocks  = (m_gpuData->size() + threads - 1) / threads;

    if (m_gpuData->sampleType() == GpuSampleType::Float32) {
        auto floatData = std::dynamic_pointer_cast<GpuFloatSignal>(m_gpuData);
        auto history = std::dynamic_pointer_cast<GpuFloatSignal>(m_historyData);
        if (!floatData || !history) {
            ERROR << "FIRFilter::run failed: invalid typed buffers for float signal." << std::endl;
            return false;
        }

        const size_t shared = (threads + m_M - 1) * sizeof(float);
        fir_inplace_kernel<<<blocks, threads, shared>>>(
            floatData->getDeviceData(),
            static_cast<int>(floatData->size()),
            history->getDeviceData(),
            m_M);
    } else if (m_gpuData->sampleType() == GpuSampleType::ComplexFloat32) {
        auto complexData = std::dynamic_pointer_cast<GpuComplexFloatSignal>(m_gpuData);
        auto history = std::dynamic_pointer_cast<GpuComplexFloatSignal>(m_historyData);
        if (!complexData || !history) {
            ERROR << "FIRFilter::run failed: invalid typed buffers for complex signal." << std::endl;
            return false;
        }

        const size_t shared = (threads + m_M - 1) * sizeof(cuComplex);
        fir_inplace_complex_kernel<<<blocks, threads, shared>>>(
            complexData->getDeviceData(),
            static_cast<int>(complexData->size()),
            history->getDeviceData(),
            m_M);
    } else {
        ERROR << "FIRFilter::run failed: unsupported signal type." << std::endl;
        return false;
    }

    const auto launchErr = cudaGetLastError();
    if (launchErr != cudaSuccess) {
        ERROR << "FIRFilter::run failed: kernel launch error: "
              << cudaGetErrorString(launchErr) << std::endl;
        return false;
    }

    DEBUG << "out:" <<  m_data->size() << std::endl; 

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
