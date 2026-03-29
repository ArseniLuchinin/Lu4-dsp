#ifndef BIT_PACKER_HPP
#define BIT_PACKER_HPP

#include <IModule.hpp>

#include <CpuByteSignal.hpp>

#include <memory>
#include <string>
#include <vector>

class BitPacker : public IModule {
public:
    BitPacker();
    ~BitPacker() override;

    bool init() override;
    bool run() override;

    void setParam(const std::string& paramName, const std::any& value) override;
    bool setData(std::shared_ptr<IData> data) override;
    std::shared_ptr<IData> getData() override;

private:
    std::string m_bitOrder = "msb-first";
    bool m_flushTail = false;
    size_t m_discardLeadingBits = 0;
    size_t m_discardLeadingBitsRemaining = 0;

    std::vector<uint8_t> m_pendingBits;

    std::shared_ptr<CpuByteSignal> m_inData;
    std::shared_ptr<CpuByteSignal> m_outData;
};

#endif // BIT_PACKER_HPP
