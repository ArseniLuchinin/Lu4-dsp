#include <VirtualTransmitter.hpp>

VirtualTransmitter& VirtualTransmitter::instance() {
    static VirtualTransmitter transmitter;
    return transmitter;
}

bool VirtualTransmitter::findTeg(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_transmitter.find(name) != m_transmitter.end();
}

bool VirtualTransmitter::checkData(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_transmitter.find(name);
    if (it == m_transmitter.end()) {
        return false;
    }

    return it->second != nullptr;
}

std::shared_ptr<IData> VirtualTransmitter::rxData(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_transmitter.find(name);
    if (it == m_transmitter.end() || it->second == nullptr) {
        return nullptr;
    }

    const auto data = it->second;
    it->second = nullptr;
    return data;
}

void VirtualTransmitter::txData(const std::shared_ptr<IData>& data, const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_transmitter[name] = data;
}
