#include <cufft.h>
#include <cufftXt.h>
#include <cuda_runtime.h>

// =========================
// Callback data
// =========================
struct CallbackData {
    cufftComplex* signal;   // новые данные (GPU)
    cufftComplex* buffer;   // overlap buffer (GPU)

    int fftSize;
    int overlap;
    int hop;
};

// =========================
// Load callback (оптимизированный)
// =========================
__device__ cufftComplex load_cb(void* dataIn,
                                size_t offset,
                                void* callerInfo,
                                void* sharedPtr)
{
    (void)dataIn;
    (void)sharedPtr;

    CallbackData* cb = (CallbackData*)callerInfo;

    int fftSize = cb->fftSize;
    int overlap = cb->overlap;
    int hop     = cb->hop;

    int batch = offset / fftSize;
    int i     = offset % fftSize;

    int windowStart = batch * hop - overlap;
    int signalIdx   = windowStart + i;

    return (signalIdx >= 0)
        ? cb->signal[signalIdx]
        : cb->buffer[signalIdx + overlap];
}

class OverlapFFT
{
public:
    OverlapFFT(int fftSize, int overlap)
        : fftSize(fftSize),
          overlap(overlap),
          hop(fftSize - overlap),
          isFirstRun(true)
    {}

    // d_signal  — новые входные данные (GPU)
    // signalSize — их размер
    // d_output  — выход FFT (GPU)
    // d_buffer  — buffer размера overlap (GPU)
    void process(cufftComplex* d_signal,
                 int signalSize,
                 cufftComplex* d_output,
                 cufftComplex* d_buffer)
    {
        // TODO: в рабочем коде проверить fftSize / overlap / signalSize и указатели.
        int numBatches = isFirstRun
            ? (signalSize - overlap) / hop
            : signalSize / hop;
        if (numBatches <= 0)
        {
            return;
        }

        // =========================
        // План FFT
        // =========================

        cufftHandle plan;
        // TODO: проверить код возврата cufftPlan1d.
        cufftPlan1d(&plan, fftSize, CUFFT_C2C, numBatches);

        // =========================
        // Подготовка callback
        // =========================

        CallbackData h_cb;
        h_cb.signal = isFirstRun ? (d_signal + overlap) : d_signal;
        h_cb.buffer = d_buffer;
        h_cb.fftSize = fftSize;
        h_cb.overlap = overlap;
        h_cb.hop = hop;

        CallbackData* d_cb = nullptr;
        // TODO: проверить коды возврата cudaMalloc / cudaMemcpy.
        cudaMalloc(&d_cb, sizeof(CallbackData));
        cudaMemcpy(d_cb, &h_cb, sizeof(CallbackData), cudaMemcpyHostToDevice);

        cufftCallbackLoadC d_load;
        // TODO: проверить коды возврата cudaMemcpyFromSymbol / cufftXtSetCallback.
        cudaMemcpyFromSymbol(&d_load, load_cb, sizeof(d_load));
        cufftXtSetCallback(plan,
                           (void**)&d_load,
                           CUFFT_CB_LD_COMPLEX,
                           (void**)&d_cb);

        // =========================
        // Запуск FFT
        // =========================

        // TODO: проверить код возврата cufftExecC2C и cudaGetLastError.
        cufftExecC2C(plan, d_signal, d_output, CUFFT_FORWARD);

        // =========================
        // Обновление buffer
        // =========================

        if (overlap > 0)
        {
            int tailStart = isFirstRun
                ? numBatches * hop
                : (numBatches - 1) * hop;

            // Для этого примера предполагается, что хвост последнего окна
            // всегда лежит непрерывно в d_signal.
            // TODO: проверить код возврата cudaMemcpy.
            cudaMemcpy(d_buffer,
                       d_signal + tailStart,
                       overlap * sizeof(cufftComplex),
                       cudaMemcpyDeviceToDevice);
        }

        // =========================
        // Cleanup
        // =========================

        // TODO: проверить коды возврата cufftDestroy / cudaFree.
        cufftDestroy(plan);
        cudaFree(d_cb);

        isFirstRun = false;
    }

private:
    int fftSize;
    int overlap;
    int hop;
    bool isFirstRun;
};
