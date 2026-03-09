#include <VirtualTransmitter.hpp>
#include <iostream>

std::map<std::string, std::shared_ptr<IData> > VirtualTransmitter::s_transmitter;
std::map<std::string, std::condition_variable> VirtualTransmitter::s_tagEvents;
std::mutex VirtualTransmitter::s_mutex;

std::condition_variable& VirtualTransmitter::getTagCvUnlocked(const std::string& name) {
    return s_tagEvents[name];
}

bool VirtualTransmitter::findTeg(const std::string& name) {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_transmitter.find(name) != s_transmitter.end();
}

bool VirtualTransmitter::checkData(const std::string& name) {
    std::lock_guard<std::mutex> lock(s_mutex);
    const auto it = s_transmitter.find(name);
    if (it == s_transmitter.end()) {
        return false;
    }

    return it->second != nullptr;
}

std::shared_ptr<IData> VirtualTransmitter::rxData(const std::string& name) {
    std::lock_guard<std::mutex> lock(s_mutex);
    const auto it = s_transmitter.find(name);
    if (it == s_transmitter.end() || it->second == nullptr) {
        return nullptr;
    }

    const auto data = it->second;
    it->second = nullptr;
    return data;
}

std::shared_ptr<IData> VirtualTransmitter::waitRxData(const std::string& name) {
    std::unique_lock<std::mutex> lock(s_mutex);
    auto& cv = getTagCvUnlocked(name);

    cv.wait(lock, [&]() {
        const auto it = s_transmitter.find(name);
        return it != s_transmitter.end() && it->second != nullptr;
    });

    auto it = s_transmitter.find(name);
    const auto data = it->second;
    it->second = nullptr;
    return data;
}

void VirtualTransmitter::txData(const std::shared_ptr<IData>& data, const std::string& name) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_transmitter[name] = data;
    std::cout << "Data " << data->getDataName() << " was transmitted to " << name << std::endl;

    auto it = s_tagEvents.find(name);
    if (it != s_tagEvents.end()) {
        it->second.notify_one();
    }
}
