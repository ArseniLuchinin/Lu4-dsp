#include <cuda_runtime.h>
#include <cuComplex.h>

#include <cstddef>

namespace {

__global__ void phaseRotatorKernel(
    const cuComplex* in,
    cuComplex* out,
    const size_t n,
    const float c,
    const float s)
{
    const size_t i = (static_cast<size_t>(blockIdx.x) * static_cast<size_t>(blockDim.x)) + static_cast<size_t>(threadIdx.x);
    if (i >= n) {
        return;
    }

    const float re = in[i].x;
    const float im = in[i].y;

    out[i].x = (re * c) + (im * s);
    out[i].y = (-re * s) + (im * c);
}

} // namespace

cudaError_t launchPhaseRotatorKernel(
    const cuComplex* in,
    cuComplex* out,
    size_t n,
    float c,
    float s,
    int blocks,
    int threads)
{
    if (n == 0) {
        return cudaSuccess;
    }

    phaseRotatorKernel<<<blocks, threads>>>(in, out, n, c, s);
    return cudaGetLastError();
}
