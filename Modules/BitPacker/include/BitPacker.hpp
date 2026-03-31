#ifndef BIT_PACKER_HPP
#define BIT_PACKER_HPP

#include <IModule.hpp>

#include <GpuByteSignal.hpp>

#include <cstddef>
#include <memory>
#include <string>

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

    std::shared_ptr<GpuByteSignal> m_pendingBitsData;
    size_t m_pendingBitsCount = 0;

    std::shared_ptr<GpuByteSignal> m_inData;
    std::shared_ptr<GpuByteSignal> m_outData;
};

#endif // BIT_PACKER_HPP
