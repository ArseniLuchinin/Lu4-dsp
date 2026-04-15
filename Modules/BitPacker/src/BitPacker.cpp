#include <BitPacker.hpp>
#include <VariablesResolve.hpp>

#include <module.hpp>

#include <cuda_runtime.h>

#include <algorithm>
#include <cstdint>
#include <limits>

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
);

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
);

namespace {

bool anyToSize(const std::any& value, size_t* out)
{
    if (!out) {
        return false;
    }

    if (value.type() == typeid(int32_t)) {
        const auto v = std::any_cast<int32_t>(value);
        if (v < 0) {
            return false;
        }
        *out = static_cast<size_t>(v);
        return true;
    }
    if (value.type() == typeid(int64_t)) {
        const auto v = std::any_cast<int64_t>(value);
        if (v < 0) {
            return false;
        }
        *out = static_cast<size_t>(v);
        return true;
    }
    if (value.type() == typeid(uint32_t)) {
        *out = static_cast<size_t>(std::any_cast<uint32_t>(value));
        return true;
    }
    if (value.type() == typeid(uint64_t)) {
        const auto v = std::any_cast<uint64_t>(value);
        if (v > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
            return false;
        }
        *out = static_cast<size_t>(v);
        return true;
    }

    return false;
}

} // namespace

IModule* createModule() {
    return new BitPacker();
}

BitPacker::BitPacker()
    : IModule({"BitPacker", "", ""})
{}

BitPacker::~BitPacker() = default;

bool BitPacker::init() {
    if (m_bitOrder != "msb-first") {
        ERROR << "BitPacker::init failed: only 'msb-first' is supported in MVP." << std::endl;
        return false;
    }

    m_discardLeadingBitsRemaining = m_discardLeadingBits;
    m_pendingBitsCount = 0;

    m_pendingBitsData = std::make_shared<GpuByteSignal>(8);
    if (!m_pendingBitsData || !m_pendingBitsData->isValid()) {
        ERROR << "BitPacker::init failed: unable to allocate pending bits GPU buffer." << std::endl;
        return false;
    }
    if (!m_pendingBitsData->setLogicalSize(0)) {
        ERROR << "BitPacker::init failed: unable to set pending bits logical size." << std::endl;
        return false;
    }

    return true;
}

bool BitPacker::setData(std::shared_ptr<IData> data) {
    m_inData = std::dynamic_pointer_cast<GpuByteSignal>(data);
    if (!m_inData) {
        ERROR << "BitPacker::setData failed: input must be GpuByteSignal with bits." << std::endl;
        return false;
    }

    if (!m_inData->isValid()) {
        ERROR << "BitPacker::setData failed: input data is invalid." << std::endl;
        return false;
    }

    return true;
}

bool BitPacker::run() {
    if (!m_inData) {
        ERROR << "BitPacker::run failed: input data is null." << std::endl;
        return false;
    }
    if (!m_pendingBitsData || !m_pendingBitsData->isValid()) {
        ERROR << "BitPacker::run failed: pending bits buffer is not initialized." << std::endl;
        return false;
    }

    const size_t inCount = m_inData->size();
    const size_t totalBits = m_pendingBitsCount + inCount;
    const size_t drop = std::min(m_discardLeadingBitsRemaining, totalBits);
    m_discardLeadingBitsRemaining -= drop;

    const size_t effectiveBits = totalBits - drop;
    const size_t fullBytes = effectiveBits / 8;
    const size_t remBits = effectiveBits % 8;
    const bool hasTail = (m_flushTail && remBits > 0);
    const size_t outBytesCount = fullBytes + (hasTail ? 1u : 0u);
    const size_t nextPendingCount = hasTail ? 0u : remBits;

    auto outData = std::make_shared<GpuByteSignal>(std::max<size_t>(size_t(1), outBytesCount));
    if (!outData || !outData->isValid()) {
        ERROR << "BitPacker::run failed: output allocation failed." << std::endl;
        return false;
    }
    if (!outData->setLogicalSize(outBytesCount)) {
        ERROR << "BitPacker::run failed: unable to set output logical size." << std::endl;
        return false;
    }

    if (outBytesCount > 0) {
        const int threads = 256;
        const int blocks = static_cast<int>((outBytesCount + static_cast<size_t>(threads) - 1) / static_cast<size_t>(threads));
        const auto packErr = launchBitPackerPackKernel(
            m_pendingBitsData->getDeviceData(),
            m_pendingBitsCount,
            m_inData->getDeviceData(),
            inCount,
            drop,
            fullBytes,
            remBits,
            hasTail,
            outData->getDeviceData(),
            outBytesCount,
            blocks,
            threads
        );
        if (packErr != cudaSuccess) {
            ERROR << "BitPacker::run failed: pack kernel launch failed: " << cudaGetErrorString(packErr) << std::endl;
            return false;
        }
    }

    if (nextPendingCount > 0) {
        const int threads = 256;
        const int blocks = static_cast<int>((nextPendingCount + static_cast<size_t>(threads) - 1) / static_cast<size_t>(threads));
        const auto pendingErr = launchBitPackerPendingKernel(
            m_pendingBitsData->getDeviceData(),
            m_pendingBitsCount,
            m_inData->getDeviceData(),
            inCount,
            drop,
            effectiveBits,
            m_pendingBitsData->getDeviceData(),
            nextPendingCount,
            blocks,
            threads
        );
        if (pendingErr != cudaSuccess) {
            ERROR << "BitPacker::run failed: pending kernel launch failed: " << cudaGetErrorString(pendingErr) << std::endl;
            return false;
        }
    }

    const auto syncErr = cudaDeviceSynchronize();
    if (syncErr != cudaSuccess) {
        ERROR << "BitPacker::run failed: kernel execution failed: " << cudaGetErrorString(syncErr) << std::endl;
        return false;
    }

    m_pendingBitsCount = nextPendingCount;
    if (!m_pendingBitsData->setLogicalSize(m_pendingBitsCount)) {
        ERROR << "BitPacker::run failed: unable to set pending bits logical size." << std::endl;
        return false;
    }

    m_outData = outData;
    return true;
}

void BitPacker::setParam(const std::string& paramName, const std::any& value) {
    if (paramName == "bit order") {
        m_bitOrder = std::any_cast<std::string>(value);
        return;
    }

    if (paramName == "flush tail") {
        m_flushTail = std::any_cast<bool>(value);
        return;
    }

    if (paramName == "discard leading bits") {
        size_t parsed = 0;
        if (!anyToSize(value, &parsed)) {
            ERROR << "BitPacker::setParam failed: invalid discard leading bits type/value." << std::endl;
            return;
        }
        m_discardLeadingBits = parsed;
        return;
    }

    ERROR << "BitPacker::setParam unknown parameter: " << paramName << std::endl;
}

std::shared_ptr<IData> BitPacker::getData() {
    return m_outData;
}
