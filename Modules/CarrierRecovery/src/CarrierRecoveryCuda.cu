#include <cuda_runtime.h>
#include <cuComplex.h>

#include <cub/cub.cuh>

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace {

template<int Order>
struct XNTransform;

template<>
struct XNTransform<2> {
    __host__ __device__ double2 operator()(const cuComplex& x) const {
        double re = static_cast<double>(x.x);
        double im = static_cast<double>(x.y);
        double newRe = (re * re) - (im * im);
        double newIm = 2.0 * re * im;
        return make_double2(newRe, newIm);
    }
};

template<>
struct XNTransform<4> {
    __host__ __device__ double2 operator()(const cuComplex& x) const {
        double re = static_cast<double>(x.x);
        double im = static_cast<double>(x.y);
        double re2 = (re * re) - (im * im);
        double im2 = 2.0 * re * im;
        double re4 = (re2 * re2) - (im2 * im2);
        double im4 = 2.0 * re2 * im2;
        return make_double2(re4, im4);
    }
};

template<>
struct XNTransform<8> {
    __host__ __device__ double2 operator()(const cuComplex& x) const {
        double re = static_cast<double>(x.x);
        double im = static_cast<double>(x.y);
        double re2 = (re * re) - (im * im);
        double im2 = 2.0 * re * im;
        double re4 = (re2 * re2) - (im2 * im2);
        double im4 = 2.0 * re2 * im2;
        double re8 = (re4 * re4) - (im4 * im4);
        double im8 = 2.0 * re4 * im4;
        return make_double2(re8, im8);
    }
};

struct SumDouble2 {
    __host__ __device__ double2 operator()(const double2& a, const double2& b) const {
        return make_double2(a.x + b.x, a.y + b.y);
    }
};

} // namespace

cudaError_t carrierRecoveryQueryReduceStorageBytes(const cuComplex* in, size_t n, int order, size_t* storageBytes)
{
    if (!storageBytes) {
        return cudaErrorInvalidValue;
    }

    if (n == 0) {
        *storageBytes = 0;
        return cudaSuccess;
    }

    *storageBytes = 0;

    switch (order) {
        case 2: {
            auto iter = cub::TransformInputIterator<double2, XNTransform<2>, const cuComplex*>(in, XNTransform<2>{});
            return cub::DeviceReduce::Reduce(nullptr, *storageBytes, iter, static_cast<double2*>(nullptr), n, SumDouble2{}, make_double2(0.0, 0.0));
        }
        case 4: {
            auto iter = cub::TransformInputIterator<double2, XNTransform<4>, const cuComplex*>(in, XNTransform<4>{});
            return cub::DeviceReduce::Reduce(nullptr, *storageBytes, iter, static_cast<double2*>(nullptr), n, SumDouble2{}, make_double2(0.0, 0.0));
        }
        case 8: {
            auto iter = cub::TransformInputIterator<double2, XNTransform<8>, const cuComplex*>(in, XNTransform<8>{});
            return cub::DeviceReduce::Reduce(nullptr, *storageBytes, iter, static_cast<double2*>(nullptr), n, SumDouble2{}, make_double2(0.0, 0.0));
        }
        default:
            return cudaErrorInvalidValue;
    }
}

cudaError_t carrierRecoveryRunReduce(
    const cuComplex* in,
    size_t n,
    int order,
    void* tempStorage,
    size_t tempStorageBytes,
    double2* outSum)
{
    if (n == 0) {
        return cudaSuccess;
    }
    if (!tempStorage || !outSum) {
        return cudaErrorInvalidValue;
    }

    switch (order) {
        case 2: {
            auto iter = cub::TransformInputIterator<double2, XNTransform<2>, const cuComplex*>(in, XNTransform<2>{});
            return cub::DeviceReduce::Reduce(tempStorage, tempStorageBytes, iter, outSum, n, SumDouble2{}, make_double2(0.0, 0.0));
        }
        case 4: {
            auto iter = cub::TransformInputIterator<double2, XNTransform<4>, const cuComplex*>(in, XNTransform<4>{});
            return cub::DeviceReduce::Reduce(tempStorage, tempStorageBytes, iter, outSum, n, SumDouble2{}, make_double2(0.0, 0.0));
        }
        case 8: {
            auto iter = cub::TransformInputIterator<double2, XNTransform<8>, const cuComplex*>(in, XNTransform<8>{});
            return cub::DeviceReduce::Reduce(tempStorage, tempStorageBytes, iter, outSum, n, SumDouble2{}, make_double2(0.0, 0.0));
        }
        default:
            return cudaErrorInvalidValue;
    }
}

__global__ void carrierRecoveryRotateKernel(
    const cuComplex* in,
    cuComplex* out,
    size_t n,
    float c,
    float s)
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

void launchCarrierRecoveryRotateKernel(
    const cuComplex* in,
    cuComplex* out,
    size_t n,
    float c,
    float s,
    int blocks,
    int threads)
{
    carrierRecoveryRotateKernel<<<blocks, threads>>>(in, out, n, c, s);
}
