#include <VirtualTX.hpp>

#include <VirtualTransmitter.hpp>
#include <module.hpp>

IModule* createModule() {
    return new VirtualTX();
}

VirtualTX::VirtualTX()
    : IModule({"VirtualTX", "VirtualTX.so", "VirtualTX.json"})
{}

VirtualTX::~VirtualTX() = default;

bool VirtualTX::init() {
    return !m_tag.empty();
}

bool VirtualTX::run() {
    if (m_tag.empty() || !m_data) {
        return false;
    }

    VirtualTransmitter::instance().txData(m_data, m_tag);
    return true;
}

void VirtualTX::setParam(const std::string& paramName, const std::any& value) {
    if (paramName == "tag") {
        m_tag = std::any_cast<std::string>(value);
    }
}

bool VirtualTX::setData(std::shared_ptr<IData> data) {
    m_data = data;
    return m_data != nullptr;
}

std::shared_ptr<IData> VirtualTX::getData() {
    return m_data;
}
