#ifndef CPU_BYTE_SIGNAL_HPP
#define CPU_BYTE_SIGNAL_HPP

#include <IData.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

class CpuByteSignal : public IData {
public:
    CpuByteSignal();
    explicit CpuByteSignal(size_t size);
    explicit CpuByteSignal(std::vector<uint8_t> bytes);

    size_t size() const override;
    size_t availableSize() const override;
    bool reserve(size_t size) override;
    bool isValid() const override;

    uint8_t* getData();
    const uint8_t* getData() const;

    std::vector<uint8_t>& bytes();
    const std::vector<uint8_t>& bytes() const;

private:
    std::vector<uint8_t> m_bytes;
};

#endif // CPU_BYTE_SIGNAL_HPP
