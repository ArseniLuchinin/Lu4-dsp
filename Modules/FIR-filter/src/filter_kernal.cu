#include <cuda_runtime.h>
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

bool FIRFilter::run(){
    const auto threads = 256;
    const auto blocks  = (m_data->size() + threads - 1) / threads;

    const size_t shared = (threads + m_M - 1) * sizeof(float);

    fir_inplace_kernel<<<blocks, threads, shared>>>(
        m_data->getDeviceData(),
        m_data->size(),
        m_history.get(),
        m_M);

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

    const auto copyErr = cudaMemcpy(
        m_history.get(),
        m_next_history.get(),
        historySize * sizeof(float),
        cudaMemcpyDeviceToDevice
    );
    if (copyErr != cudaSuccess) {
        ERROR << "FIRFilter::run failed: history update copy error: "
              << cudaGetErrorString(copyErr) << std::endl;
        return false;
    }

    return true;
}
