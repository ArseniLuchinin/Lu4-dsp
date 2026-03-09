#include <VirtualRX.hpp>
#include <VirtualTransmitter.hpp>

VirtualRX::VirtualRX(){}

VirtualRX::~VirtualRX() = default;

bool VirtualRX::setTag(const std::string& tag) {
    m_tag = tag;
    return !m_tag.empty();
}

std::shared_ptr<IData> VirtualRX::rxData() {
    if (m_tag.empty()) {
        return nullptr;
    }

    VirtualTransmitter transmitter;
    return transmitter.waitRxData(m_tag);
}
