#include <ConveyorFactory.hpp>

#include <boost/system/error_code.hpp>

#include <limits>
#include <stdexcept>

using boost::json::object;
using boost::json::value;

ConveyorFactory::ConveyorFactory(ModuleFactory& moduleFactory)
    : m_factory(moduleFactory)
    , logger(boost::log::keywords::channel = "ConveyorFactory")
{}

ConveyorFactory::BuildResult ConveyorFactory::createFromJsonString(
    const std::string& jsonStr
) {
    boost::system::error_code ec;
    auto parsed = boost::json::parse(jsonStr, ec);
    if (ec) {
        throw std::runtime_error(
            std::string("JSON parse error: ") + ec.message()
        );
    }

    const auto* obj = parsed.if_object();
    if (!obj) {
        throw std::runtime_error("Conveyor JSON is not an object");
    }

    return createFromJsonObject(*obj);
}

ConveyorFactory::BuildResult ConveyorFactory::createFromJsonObject(
    const object& conveyorObj
) {
    auto nameIt = conveyorObj.find("name");
    if (nameIt == conveyorObj.end() || !nameIt->value().is_string()) {
        throw std::runtime_error("Conveyor 'name' is required");
    }

    const std::string name = nameIt->value().as_string().c_str();
    BuildResult result;
    result.name = name;
    result.conveyor = std::make_shared<Conveyor>(name);

    auto modulesIt = conveyorObj.find("modules");
    if (modulesIt == conveyorObj.end() || !modulesIt->value().is_array()) {
        throw std::runtime_error("Conveyor 'modules' must be an array");
    }

    const auto& modules = modulesIt->value().as_array();
    for (const auto& modVal : modules) {
        const auto* modObj = modVal.if_object();
        if (!modObj) {
            throw std::runtime_error("Module entry is not an object");
        }

        if (!buildModule(result, *modObj)) {
            throw std::runtime_error(
                "Failed to build module for conveyor: " + name
            );
        }
    }

    return result;
}

bool ConveyorFactory::buildModule(
    BuildResult& result,
    const object& moduleObj
) {
    auto nameIt = moduleObj.find("name");
    if (nameIt == moduleObj.end() || !nameIt->value().is_string()) {
        ERROR << "Module 'name' is required." << std::endl;
        return false;
    }

    const std::string moduleName = nameIt->value().as_string().c_str();
    std::shared_ptr<IModule> module(m_factory.createModule(moduleName));
    if (!module) {
        ERROR << "Failed to create module: " << moduleName << std::endl;
        return false;
    }

    if (auto paramsIt = moduleObj.find("params"); paramsIt != moduleObj.end()) {
        if (!paramsIt->value().is_object()) {
            ERROR << "'params' must be an object for module: "
                  << moduleName << std::endl;
            return false;
        }

        const auto& params = paramsIt->value().as_object();
        for (const auto& kv : params) {
            const std::string paramName = std::string(kv.key());
            module->setParam(paramName, jsonToAny(kv.value()));
        }
    }

    if (moduleName == "VirtualRX") {
        result.hasVirtualRx = true;
        auto paramsIt = moduleObj.find("params");
        if (paramsIt != moduleObj.end() && paramsIt->value().is_object()) {
            const auto& params = paramsIt->value().as_object();
            auto tagIt = params.find("tag");
            if (tagIt != params.end() && tagIt->value().is_string()) {
                result.rxTags.push_back(tagIt->value().as_string().c_str());
            }
        }
    }

    result.conveyor->addModule(module);
    return true;
}

std::any ConveyorFactory::jsonToAny(const value& v) {
    if (v.is_string()) {
        return std::string(v.as_string().c_str());
    }
    if (v.is_bool()) {
        return v.as_bool();
    }
    if (v.is_int64()) {
        const auto val = v.as_int64();
        return static_cast<int32_t>(val);
    }
    if (v.is_uint64()) {
        const auto val = v.as_uint64();
        if (val > static_cast<uint64_t>(std::numeric_limits<int32_t>::max())) {
            return static_cast<double>(val);
        }
        return static_cast<int32_t>(val);
    }
    if (v.is_double()) {
        return v.as_double();
    }
    if (v.is_null()) {
        return std::any{};
    }

    return std::any{};
}
