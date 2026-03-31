#include <cuda_runtime.h>
#include <cuComplex.h>

#include <cstddef>
#include <cstdint>

namespace {

__global__ void qpskDecisionKernel(
    const cuComplex* in,
    uint8_t* outBits,
    const size_t symbolsCount)
{
    const size_t i = (static_cast<size_t>(blockIdx.x) * static_cast<size_t>(blockDim.x)) + static_cast<size_t>(threadIdx.x);
    if (i >= symbolsCount) {
        return;
    }

    const uint8_t bit0 = (in[i].x > 0.0f) ? 0u : 1u;
    const uint8_t bit1 = (in[i].y > 0.0f) ? 0u : 1u;
    outBits[(2 * i) + 0] = bit0;
    outBits[(2 * i) + 1] = bit1;
}

} // namespace

cudaError_t launchQpskDecisionKernel(
    const cuComplex* in,
    uint8_t* outBits,
    size_t symbolsCount,
    int blocks,
    int threads)
{
    if (symbolsCount == 0) {
        return cudaSuccess;
    }

    qpskDecisionKernel<<<blocks, threads>>>(in, outBits, symbolsCount);
    return cudaGetLastError();
}
