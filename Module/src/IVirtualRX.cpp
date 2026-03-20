#include <IVirtualRX.hpp>
#include <VirtualTransmitter.hpp>

IVirtualRX::IVirtualRX(){}

IVirtualRX::~IVirtualRX() = default;

bool IVirtualRX::setTag(const std::string& tag) {
    m_tag = tag;
    return !m_tag.empty();
}

std::shared_ptr<IData> IVirtualRX::rxData() {
    if (m_tag.empty()) {
        return nullptr;
    }

    VirtualTransmitter transmitter;
    return transmitter.waitRxData(m_tag);
}
