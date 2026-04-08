#include <IVirtualRX.hpp>
#include <VirtualTransmitter.hpp>

IVirtualRX::IVirtualRX(){}

IVirtualRX::~IVirtualRX() = default;

bool IVirtualRX::setTag(const std::string& tag) {
    if (tag.empty()) {
        return false;
    }

    m_tag = tag;

    // Автоматически регистрируем RX для этого тега (инкремент счётчика)
    VirtualTransmitter::registerRx(tag);

    return true;
}

std::shared_ptr<IData> IVirtualRX::rxData() {
    if (m_tag.empty()) {
        return nullptr;
    }

    VirtualTransmitter transmitter;
    return transmitter.waitRxData(m_tag);
}
