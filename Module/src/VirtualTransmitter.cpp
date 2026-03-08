#include <VirtualTransmitter.hpp>

bool VirtualTransmitter::findTeg(std::string name) {
    return s_transmitter.find(name) != s_transmitter.end();
}

bool VirtualTransmitter::checkData(std::string name) {
    if(findTeg(name)){
        return s_transmitter[name] != nullptr;
    }
}

std::shared_ptr<IData> VirtualTransmitter::rxData(const std::string& name) {
    if(checkData(name)){
        const auto data = s_transmitter[name];
        s_transmitter[name] = nullptr;
        return data;
    }

    return nullptr;
}

void VirtualTransmitter::txData(const std::shared_ptr<IData> data, const std::string& name) {
    s_transmitter[name] = data;
}