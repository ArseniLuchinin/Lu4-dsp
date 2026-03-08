#include <IVirtualRxModule.hpp>
#include <VirtualTransmitter.hpp>

IVirtualRxModule::IVirtualRxModule()
    : IModule({"IVirtualRxModule", "IVirtualRxModule", "IVirtualRxModule"})
{}

IVirtualRxModule::~IVirtualRxModule() = default;

bool IVirtualRxModule::setTag(const std::string& tag) {
    m_tag = tag;
    return !m_tag.empty();
}

std::shared_ptr<IData> IVirtualRxModule::rxData() {
    if (m_tag.empty()) {
        return nullptr;
    }

    VirtualTransmitter transmitter;
    return transmitter.waitRxData(m_tag);
}
