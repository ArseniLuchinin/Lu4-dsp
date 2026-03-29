#include <BitPacker.hpp>
#include <VariablesResolve.hpp>

#include <module.hpp>

#include <cstdint>
#include <limits>
#include <vector>

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
    return true;
}

bool BitPacker::setData(std::shared_ptr<IData> data) {
    m_inData = std::dynamic_pointer_cast<CpuByteSignal>(data);
    if (!m_inData) {
        ERROR << "BitPacker::setData failed: input must be CpuByteSignal with bits." << std::endl;
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

    std::vector<uint8_t> allBits;
    allBits.reserve(m_pendingBits.size() + m_inData->size());
    allBits.insert(allBits.end(), m_pendingBits.begin(), m_pendingBits.end());

    const auto& inBits = m_inData->bytes();
    for (const uint8_t bit : inBits) {
        allBits.push_back(bit & 0x01u);
    }

    if (m_discardLeadingBitsRemaining > 0 && !allBits.empty()) {
        const size_t drop = std::min(m_discardLeadingBitsRemaining, allBits.size());
        allBits.erase(allBits.begin(), allBits.begin() + static_cast<long>(drop));
        m_discardLeadingBitsRemaining -= drop;
    }

    std::vector<uint8_t> outBytes;
    outBytes.reserve(allBits.size() / 8 + 1);

    size_t idx = 0;
    while ((idx + 8) <= allBits.size()) {
        uint8_t byte = 0u;
        for (size_t i = 0; i < 8; ++i) {
            byte = static_cast<uint8_t>((byte << 1u) | allBits[idx + i]);
        }
        outBytes.push_back(byte);
        idx += 8;
    }

    m_pendingBits.assign(allBits.begin() + static_cast<long>(idx), allBits.end());

    if (m_flushTail && !m_pendingBits.empty()) {
        uint8_t byte = 0u;
        for (const uint8_t bit : m_pendingBits) {
            byte = static_cast<uint8_t>((byte << 1u) | (bit & 0x01u));
        }
        byte = static_cast<uint8_t>(byte << static_cast<uint8_t>(8 - m_pendingBits.size()));
        outBytes.push_back(byte);
        m_pendingBits.clear();
    }

    m_outData = std::make_shared<CpuByteSignal>(std::move(outBytes));
    return true;
}

void BitPacker::setParam(const std::string& paramName, const std::any& value) {
    const std::any resolved = resolveParamValue(value);

    if (paramName == "bit order") {
        m_bitOrder = std::any_cast<std::string>(resolved);
        return;
    }

    if (paramName == "flush tail") {
        m_flushTail = std::any_cast<bool>(resolved);
        return;
    }

    if (paramName == "discard leading bits") {
        size_t parsed = 0;
        if (!anyToSize(resolved, &parsed)) {
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
