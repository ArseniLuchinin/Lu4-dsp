#include <cuda_runtime.h>
#include <cuComplex.h>

#include <cstddef>

namespace {

__global__ void decimatorKernel(
    const cuComplex* in,
    cuComplex* out,
    const size_t outCount,
    const size_t phase,
    const size_t sps)
{
    const size_t idx = (static_cast<size_t>(blockIdx.x) * static_cast<size_t>(blockDim.x)) + static_cast<size_t>(threadIdx.x);
    if (idx >= outCount) {
        return;
    }

    const size_t inIdx = phase + (idx * sps);
    out[idx] = in[inIdx];
}

} // namespace

cudaError_t launchDecimatorKernel(
    const cuComplex* in,
    cuComplex* out,
    size_t outCount,
    size_t phase,
    size_t sps,
    int blocks,
    int threads)
{
    if (outCount == 0) {
        return cudaSuccess;
    }

    decimatorKernel<<<blocks, threads>>>(in, out, outCount, phase, sps);
    return cudaGetLastError();
}
