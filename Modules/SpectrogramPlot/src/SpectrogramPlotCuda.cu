#include <cuda_runtime.h>

#include <cmath>
#include <cstddef>

namespace {

__device__ inline float clamp01(float value)
{
    return fminf(fmaxf(value, 0.0f), 1.0f);
}

__device__ inline unsigned char toByte(float value)
{
    const int rounded = static_cast<int>(roundf(value));
    return static_cast<unsigned char>(max(0, min(255, rounded)));
}

__global__ void renderSpectrogramKernel(
    const float* input,
    unsigned char* outputBgr,
    size_t rows,
    size_t cols,
    size_t colOffset,
    size_t totalInputCols,
    float minValue,
    float maxValue,
    bool hasMaskBelowDb,
    float maskBelowDb,
    const unsigned char* colorLut)
{
    const size_t gid = (static_cast<size_t>(blockIdx.x) * blockDim.x) + threadIdx.x;
    const size_t total = rows * cols;
    if (gid >= total) {
        return;
    }

    const size_t y = gid / cols;
    const size_t x = gid - (y * cols);
    const size_t xIn = colOffset + x;

    // Проверка границ
    if (xIn >= totalInputCols) {
        return;
    }

    const float raw = input[(y * totalInputCols) + xIn];

    const float range = fmaxf(maxValue - minValue, 1.0e-12f);
    const float normalized = clamp01((raw - minValue) / range);

    const size_t dstY = rows - 1 - y;
    const size_t outBase = ((dstY * cols) + x) * 3;

    if (hasMaskBelowDb && raw < maskBelowDb) {
        const float base = powf(normalized, 1.6f);
        // Keep masked region in a cold blue/cyan range to avoid red/maroon background.
        outputBgr[outBase + 0] = toByte(24.0f + (40.0f * base)); // B
        outputBgr[outBase + 1] = toByte(12.0f + (28.0f * base)); // G
        outputBgr[outBase + 2] = toByte(4.0f + (10.0f * base));  // R
        return;
    }

    const float gammaCorrected = powf(normalized, 0.8f);
    const int lutIndex = max(0, min(255, static_cast<int>(roundf(gammaCorrected * 255.0f))));
    const size_t lutBase = static_cast<size_t>(lutIndex) * 3;

    outputBgr[outBase + 0] = colorLut[lutBase + 0];
    outputBgr[outBase + 1] = colorLut[lutBase + 1];
    outputBgr[outBase + 2] = colorLut[lutBase + 2];
}

} // namespace

bool renderSpectrogramImageCuda(
    const float* input,
    unsigned char* outputBgr,
    size_t rows,
    size_t cols,
    size_t colOffset,
    size_t inputCols,
    float minValue,
    float maxValue,
    bool hasMaskBelowDb,
    float maskBelowDb,
    const unsigned char* colorLut)
{
    if (!input || !outputBgr || !colorLut) {
        return false;
    }

    const size_t total = rows * cols;
    if (total == 0) {
        return true;
    }

    constexpr int threads = 256;
    const int blocks = static_cast<int>((total + static_cast<size_t>(threads) - 1) / static_cast<size_t>(threads));
    renderSpectrogramKernel<<<blocks, threads>>>(
        input,
        outputBgr,
        rows,
        cols,
        colOffset,
        inputCols,
        minValue,
        maxValue,
        hasMaskBelowDb,
        maskBelowDb,
        colorLut);

    return cudaGetLastError() == cudaSuccess;
}
