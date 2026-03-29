#include <CpuByteSignal.hpp>
#include <utility>

CpuByteSignal::CpuByteSignal()
    : IData("CPU byte signal")
{}

CpuByteSignal::CpuByteSignal(size_t size)
    : IData("CPU byte signal")
    , m_bytes(size)
{}

CpuByteSignal::CpuByteSignal(std::vector<uint8_t> bytes)
    : IData("CPU byte signal")
    , m_bytes(std::move(bytes))
{}

size_t CpuByteSignal::size() const {
    return m_bytes.size();
}

size_t CpuByteSignal::availableSize() const {
    return m_bytes.size();
}

bool CpuByteSignal::reserve(size_t size) {
    m_bytes.reserve(size);
    return true;
}

bool CpuByteSignal::isValid() const {
    return true;
}

uint8_t* CpuByteSignal::getData() {
    return m_bytes.empty() ? nullptr : m_bytes.data();
}

const uint8_t* CpuByteSignal::getData() const {
    return m_bytes.empty() ? nullptr : m_bytes.data();
}

std::vector<uint8_t>& CpuByteSignal::bytes() {
    return m_bytes;
}

const std::vector<uint8_t>& CpuByteSignal::bytes() const {
    return m_bytes;
}
