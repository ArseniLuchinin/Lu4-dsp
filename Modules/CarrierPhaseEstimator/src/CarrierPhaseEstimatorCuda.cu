#include <cuda_runtime.h>
#include <cuComplex.h>

#include <cub/cub.cuh>

#include <cstddef>

namespace {

struct X4Transform {
    __host__ __device__ double2 operator()(const cuComplex& x) const {
        const double re = static_cast<double>(x.x);
        const double im = static_cast<double>(x.y);

        const double re2 = (re * re) - (im * im);
        const double im2 = 2.0 * re * im;

        const double re4 = (re2 * re2) - (im2 * im2);
        const double im4 = 2.0 * re2 * im2;
        return make_double2(re4, im4);
    }
};

struct SumDouble2 {
    __host__ __device__ double2 operator()(const double2& a, const double2& b) const {
        return make_double2(a.x + b.x, a.y + b.y);
    }
};

} // namespace

cudaError_t carrierPhaseQueryReduceStorageBytes(const cuComplex* in, size_t n, size_t* storageBytes)
{
    if (!storageBytes) {
        return cudaErrorInvalidValue;
    }

    if (n == 0) {
        *storageBytes = 0;
        return cudaSuccess;
    }

    *storageBytes = 0;
    auto transformIter = cub::TransformInputIterator<double2, X4Transform, const cuComplex*>(in, X4Transform{});
    return cub::DeviceReduce::Reduce(
        nullptr,
        *storageBytes,
        transformIter,
        static_cast<double2*>(nullptr),
        n,
        SumDouble2{},
        make_double2(0.0, 0.0)
    );
}

cudaError_t carrierPhaseRunReduce(
    const cuComplex* in,
    size_t n,
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

    auto transformIter = cub::TransformInputIterator<double2, X4Transform, const cuComplex*>(in, X4Transform{});
    return cub::DeviceReduce::Reduce(
        tempStorage,
        tempStorageBytes,
        transformIter,
        outSum,
        n,
        SumDouble2{},
        make_double2(0.0, 0.0)
    );
}
