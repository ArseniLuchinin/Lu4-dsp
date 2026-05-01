#include <VirtualTX.hpp>
#include <VariablesResolve.hpp>

#include <VirtualTransmitter.hpp>
#include <EmptyContainer.hpp>
#include <module.hpp>

IModule* createModule() {
    return new VirtualTX();
}

VirtualTX::VirtualTX()
    : IModule({"VirtualTX", "libVirtualTX-module.so", "module.json"})
{}

VirtualTX::~VirtualTX() {
    if (!m_tag.empty()) {
        VirtualTransmitter transmitter;
        transmitter.txData(std::make_shared<EmptyContainer>(), m_tag);
    }
}

bool VirtualTX::init() {
    if (m_tag.empty()) {
        ERROR << "VirtualTX::init failed: tag is empty." << std::endl;
        return false;
    }

    INFO << "VirtualTX initialized with tag: " << m_tag << std::endl;
    return true;
}

bool VirtualTX::run() {
    if (m_tag.empty()) {
        ERROR << "VirtualTX::run failed: tag is empty." << std::endl;
        return false;
    }

    if (!m_data) {
        ERROR << "VirtualTX::run failed: input data is null." << std::endl;
        return false;
    }

    VirtualTransmitter transmitter;
    transmitter.txData(m_data, m_tag);
    INFO << "VirtualTX transmitted data by tag: " << m_tag
         << ", size: " << m_data->size()
         << ", type: " << m_data->getDataName() << std::endl;
    return true;
}

void VirtualTX::setParam(const std::string& paramName, const std::any& value) {
    if (paramName == "tag") {
        m_tag = std::any_cast<std::string>(value);

        // Валидация: тег может принадлежать только одному TX
        if (!VirtualTransmitter::registerTx(m_tag)) {
            ERROR << "VirtualTX::setParam failed: tag='" << m_tag
                  << "' is already registered by another TX." << std::endl;
            return;
        }

        INFO << "VirtualTX tag set to: " << m_tag << std::endl;
        return;
    }

    ERROR << "VirtualTX::setParam unknown parameter: " << paramName << std::endl;
}

bool VirtualTX::setData(std::shared_ptr<IData> data) {
    m_data = data;
    if (!m_data) {
        ERROR << "VirtualTX::setData failed: received null data." << std::endl;
        return false;
    }

    INFO << "VirtualTX accepted input data: size=" << m_data->size()
         << ", type=" << m_data->getDataName() << std::endl;
    return true;
}

std::shared_ptr<IData> VirtualTX::getData() {
    return m_data;
}
