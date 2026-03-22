#include <cufft.h>
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

    int baseOffset; // overlap (первый запуск) или 0 (дальше)
};

// =========================
// Load callback (оптимизированный)
// =========================
__device__ cufftComplex load_cb(void* dataIn,
                                size_t offset,
                                void* callerInfo,
                                void* sharedPtr)
{
    CallbackData* cb = (CallbackData*)callerInfo;

    int fftSize = cb->fftSize;
    int overlap = cb->overlap;
    int hop     = cb->hop;

    int batch = offset / fftSize;
    int i     = offset % fftSize;

    // Переносим -overlap заранее
    int base = cb->baseOffset + batch * hop - overlap;
    int idx  = base + i;

    // Выбираем адрес (одно чтение!)
    cufftComplex* ptr = (i < overlap)
        ? (cb->buffer + i)
        : (cb->signal + idx);

    return *ptr;
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
        // =========================
        // Определяем параметры
        // =========================

        int baseOffset = isFirstRun ? overlap : 0;

        int numBatches;
        if (isFirstRun)
        {
            // ⚠️ предполагается что данных достаточно
            numBatches = (signalSize - overlap) / hop;
        }
        else
        {
            numBatches = signalSize / hop;
        }

        // =========================
        // План FFT
        // =========================

        cufftHandle plan;
        cufftPlan1d(&plan, fftSize, CUFFT_C2C, numBatches);

        // =========================
        // Подготовка callback
        // =========================

        CallbackData h_cb;
        h_cb.signal = d_signal;
        h_cb.buffer = d_buffer;
        h_cb.fftSize = fftSize;
        h_cb.overlap = overlap;
        h_cb.hop = hop;
        h_cb.baseOffset = baseOffset;

        CallbackData* d_cb;
        cudaMalloc(&d_cb, sizeof(CallbackData));
        cudaMemcpy(d_cb, &h_cb, sizeof(CallbackData), cudaMemcpyHostToDevice);

        cufftCallbackLoadC d_load;
        cudaMemcpyFromSymbol(&d_load, load_cb, sizeof(d_load));

        cufftXtSetCallback(plan,
                           (void**)&d_load,
                           CUFFT_CB_LD_COMPLEX,
                           (void**)&d_cb);

        // =========================
        // Запуск FFT
        // =========================

        cufftExecC2C(plan, d_signal, d_output, CUFFT_FORWARD);

        // =========================
        // Обновление buffer
        // =========================

        int lastBatch = numBatches - 1;

        int baseIdx;
        if (isFirstRun)
        {
            baseIdx = overlap + lastBatch * hop;
        }
        else
        {
            baseIdx = lastBatch * hop;
        }

        // Копируем хвост последнего окна
        cudaMemcpy(d_buffer,
                   d_signal + baseIdx + (fftSize - overlap),
                   overlap * sizeof(cufftComplex),
                   cudaMemcpyDeviceToDevice);

        // =========================
        // Cleanup
        // =========================

        cufftDestroy(plan);
        cudaFree(d_cb);

        // =========================
        // Переход в steady-state
        // =========================

        isFirstRun = false;
    }

private:
    int fftSize;
    int overlap;
    int hop;

    bool isFirstRun;
};