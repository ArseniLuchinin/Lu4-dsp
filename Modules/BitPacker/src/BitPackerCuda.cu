#include <cuda_runtime.h>

#include <cstddef>
#include <cstdint>

namespace {

__device__ uint8_t bitAt(
    const uint8_t* pendingBits,
    const size_t pendingCount,
    const uint8_t* inBits,
    const size_t logicalIdx,
    const size_t drop)
{
    const size_t src = logicalIdx + drop;
    if (src < pendingCount) {
        return static_cast<uint8_t>(pendingBits[src] & 0x01u);
    }
    return static_cast<uint8_t>(inBits[src - pendingCount] & 0x01u);
}

__global__ void packBytesKernel(
    const uint8_t* pendingBits,
    size_t pendingCount,
    const uint8_t* inBits,
    size_t inCount,
    size_t drop,
    size_t fullBytes,
    size_t remBits,
    bool hasTail,
    uint8_t* outBytes,
    size_t outBytesCount)
{
    (void)inCount;
    const size_t idx = (static_cast<size_t>(blockIdx.x) * static_cast<size_t>(blockDim.x)) + static_cast<size_t>(threadIdx.x);
    if (idx >= outBytesCount) {
        return;
    }

    const bool isTail = hasTail && (idx == fullBytes);
    const size_t bitsThisByte = isTail ? remBits : 8u;
    uint8_t byte = 0u;

    for (size_t i = 0; i < bitsThisByte; ++i) {
        const size_t logicalBitIdx = (idx * 8u) + i;
        byte = static_cast<uint8_t>((byte << 1u) | bitAt(pendingBits, pendingCount, inBits, logicalBitIdx, drop));
    }

    if (isTail && bitsThisByte < 8u) {
        byte = static_cast<uint8_t>(byte << static_cast<uint8_t>(8u - bitsThisByte));
    }

    outBytes[idx] = byte;
}

__global__ void updatePendingKernel(
    const uint8_t* pendingBits,
    size_t pendingCount,
    const uint8_t* inBits,
    size_t inCount,
    size_t drop,
    size_t effectiveBits,
    uint8_t* outPending,
    size_t nextPendingCount)
{
    (void)inCount;
    const size_t idx = (static_cast<size_t>(blockIdx.x) * static_cast<size_t>(blockDim.x)) + static_cast<size_t>(threadIdx.x);
    if (idx >= nextPendingCount) {
        return;
    }

    const size_t logicalBitIdx = (effectiveBits - nextPendingCount) + idx;
    outPending[idx] = bitAt(pendingBits, pendingCount, inBits, logicalBitIdx, drop);
}

} // namespace

cudaError_t launchBitPackerPackKernel(
    const uint8_t* pendingBits,
    size_t pendingCount,
    const uint8_t* inBits,
    size_t inCount,
    size_t drop,
    size_t fullBytes,
    size_t remBits,
    bool hasTail,
    uint8_t* outBytes,
    size_t outBytesCount,
    int blocks,
    int threads
)
{
    if (outBytesCount == 0) {
        return cudaSuccess;
    }

    packBytesKernel<<<blocks, threads>>>(
        pendingBits,
        pendingCount,
        inBits,
        inCount,
        drop,
        fullBytes,
        remBits,
        hasTail,
        outBytes,
        outBytesCount
    );
    return cudaGetLastError();
}

cudaError_t launchBitPackerPendingKernel(
    const uint8_t* pendingBits,
    size_t pendingCount,
    const uint8_t* inBits,
    size_t inCount,
    size_t drop,
    size_t effectiveBits,
    uint8_t* nextPendingBits,
    size_t nextPendingCount,
    int blocks,
    int threads
)
{
    if (nextPendingCount == 0) {
        return cudaSuccess;
    }

    updatePendingKernel<<<blocks, threads>>>(
        pendingBits,
        pendingCount,
        inBits,
        inCount,
        drop,
        effectiveBits,
        nextPendingBits,
        nextPendingCount
    );
    return cudaGetLastError();
}
