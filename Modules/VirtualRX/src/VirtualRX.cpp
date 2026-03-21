#include <VirtualRX.hpp>
#include <VariablesResolve.hpp>

#include <module.hpp>

IModule* createModule() {
    return new VirtualRX();
}

VirtualRX::VirtualRX()
    : IModule({"VirtualRX", "VirtualRX.so", "VirtualRX.json"})
    , IVirtualRX()
{}

VirtualRX::~VirtualRX() = default;

bool VirtualRX::init() {
    if (m_tag.empty()) {
        ERROR << "VirtualRX::init failed: tag is empty." << std::endl;
        return false;
    }

    INFO << "VirtualRX initialized with tag: " << m_tag << std::endl;
    return true;
}

bool VirtualRX::run() {
    if (m_tag.empty()) {
        ERROR << "VirtualRX::run failed: tag is empty." << std::endl;
        return false;
    }

    auto data = rxData();
    if (!data) {
        ERROR << "VirtualRX::run failed: received null data." << std::endl;
        return false;
    }

    m_data = data;
    INFO << "VirtualRX received data by tag: " << m_tag
         << ", size: " << m_data->size()
         << ", type: " << m_data->getDataName() << std::endl;
    return true;
}

void VirtualRX::setParam(const std::string& paramName, const std::any& value) {
    const std::any resolved = resolveParamValue(value);
    if (paramName == "tag") {
        const auto tag = std::any_cast<std::string>(resolved);
        if (!setTag(tag)) {
            ERROR << "VirtualRX::setParam failed: tag is empty." << std::endl;
            return;
        }
        INFO << "VirtualRX tag set to: " << m_tag << std::endl;
        return;
    }

    ERROR << "VirtualRX::setParam unknown parameter: " << paramName << std::endl;
}

bool VirtualRX::setData(std::shared_ptr<IData> data) {
    (void)data;
    ERROR << "VirtualRX::setData is not supported for source module." << std::endl;
    return false;
}

std::shared_ptr<IData> VirtualRX::getData() {
    return m_data;
}
