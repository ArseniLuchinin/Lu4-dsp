#include <JsonReader.hpp>

#include <VirtualTransmitter.hpp>
#include <Variables.hpp>
#include <EmptyContainer.hpp>

#include <chrono>
#include <fstream>
#include <limits>
#include <sstream>
#include <boost/system/error_code.hpp>

using boost::json::array;
using boost::json::object;
using boost::json::value;

JsonReader::JsonReader(const std::string& configPath)
    : m_configPath(configPath)
    , m_factory(".")
    , logger(boost::log::keywords::channel = "JsonReader")
{}

bool JsonReader::load() {
    std::ifstream file(m_configPath);
    if (!file) {
        ERROR << "Failed to open config: " << m_configPath << std::endl;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    boost::system::error_code ec;
    m_root = boost::json::parse(buffer.str(), ec);
    if (ec) {
        ERROR << "JSON parse error: " << ec.message() << std::endl;
        return false;
    }

    m_loaded = true;
    return true;
}

bool JsonReader::run() {
    if (!m_loaded) {
        ERROR << "JsonReader::run: config not loaded." << std::endl;
        return false;
    }

    const auto* rootObj = m_root.if_object();
    if (!rootObj) {
        ERROR << "JsonReader::run: root is not an object." << std::endl;
        return false;
    }

    // Variables (TOML)
    if (auto it = rootObj->find("variables"); it != rootObj->end()) {
        if (!it->value().is_string()) {
            ERROR << "JsonReader::run: 'variables' must be a string path." << std::endl;
            return false;
        }

        const std::string varsPath = it->value().as_string().c_str();
        if (!Variables::instance().load(varsPath)) {
            ERROR << "JsonReader::run: failed to load variables: " << varsPath << std::endl;
            return false;
        }
    }

    if (!buildConveyors()) {
        return false;
    }

    // Start every conveyor in its own thread.
    for (auto& runtime : m_conveyors) {
        runtime.thread = std::thread([this, &runtime]() {
            const auto start = std::chrono::steady_clock::now();
            if (!runtime.conveyor->init()) {
                ERROR << "Error init conveyor: " << runtime.name << std::endl;
                return;
            }
            while (runtime.conveyor->run()) {
            }
            const auto end = std::chrono::steady_clock::now();
            runtime.elapsedSeconds =
                std::chrono::duration<double>(end - start).count();
        });
    }

    // Wait for conveyors without VirtualRX first (TX path finishes).
    for (auto& runtime : m_conveyors) {
        if (!runtime.hasVirtualRx && runtime.thread.joinable()) {
            runtime.thread.join();
        }
    }

    // Send end-of-stream to any VirtualRX by tag to unblock receivers.
    for (const auto& runtime : m_conveyors) {
        if (!runtime.hasVirtualRx) {
            continue;
        }
        for (const auto& tag : runtime.rxTags) {
            VirtualTransmitter transmitter;
            transmitter.txData(std::make_shared<EmptyContainer>(), tag);
        }
    }

    // Now wait for RX conveyors to finish.
    for (auto& runtime : m_conveyors) {
        if (runtime.hasVirtualRx && runtime.thread.joinable()) {
            runtime.thread.join();
        }
    }

    for (const auto& runtime : m_conveyors) {
        INFO << "Conveyor '" << runtime.name << "' total time: "
             << runtime.elapsedSeconds << " s" << std::endl;
    }

    return true;
}

bool JsonReader::buildConveyors() {
    const auto* rootObj = m_root.if_object();
    if (!rootObj) {
        ERROR << "JsonReader::buildConveyors: root is not an object." << std::endl;
        return false;
    }

    auto it = rootObj->find("conveyors");
    if (it == rootObj->end() || !it->value().is_array()) {
        ERROR << "JsonReader::buildConveyors: 'conveyors' must be an array." << std::endl;
        return false;
    }

    const auto& conveyors = it->value().as_array();
    for (const auto& item : conveyors) {
        const auto* obj = item.if_object();
        if (!obj) {
            ERROR << "JsonReader::buildConveyors: conveyor is not an object." << std::endl;
            return false;
        }

        if (!buildConveyor(*obj)) {
            return false;
        }
    }

    return true;
}

bool JsonReader::buildConveyor(const object& conveyorObj) {
    auto nameIt = conveyorObj.find("name");
    if (nameIt == conveyorObj.end() || !nameIt->value().is_string()) {
        ERROR << "JsonReader::buildConveyor: 'name' is required." << std::endl;
        return false;
    }

    const std::string name = nameIt->value().as_string().c_str();
    auto conv = std::make_shared<Conveyor>(name);

    auto modulesIt = conveyorObj.find("modules");
    if (modulesIt == conveyorObj.end() || !modulesIt->value().is_array()) {
        ERROR << "JsonReader::buildConveyor: 'modules' must be an array." << std::endl;
        return false;
    }

    ConveyorRuntime runtime;
    runtime.conveyor = conv;
    runtime.name = name;

    const auto& modules = modulesIt->value().as_array();
    for (const auto& modVal : modules) {
        const auto* modObj = modVal.if_object();
        if (!modObj) {
            ERROR << "JsonReader::buildConveyor: module is not an object." << std::endl;
            return false;
        }

        if (!buildModule(runtime, *modObj)) {
            return false;
        }
    }

    m_conveyors.push_back(std::move(runtime));
    return true;
}

bool JsonReader::buildModule(ConveyorRuntime& runtime, const object& moduleObj) {
    auto nameIt = moduleObj.find("name");
    if (nameIt == moduleObj.end() || !nameIt->value().is_string()) {
        ERROR << "JsonReader::buildModule: module 'name' is required." << std::endl;
        return false;
    }

    const std::string moduleName = nameIt->value().as_string().c_str();
    std::shared_ptr<IModule> module(m_factory.createModule(moduleName));
    if (!module) {
        ERROR << "JsonReader::buildModule: failed to create module: " << moduleName << std::endl;
        return false;
    }

    if (auto paramsIt = moduleObj.find("params"); paramsIt != moduleObj.end()) {
        if (!paramsIt->value().is_object()) {
            ERROR << "JsonReader::buildModule: 'params' must be an object for module: "
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
        runtime.hasVirtualRx = true;
        auto paramsIt = moduleObj.find("params");
        if (paramsIt != moduleObj.end() && paramsIt->value().is_object()) {
            const auto& params = paramsIt->value().as_object();
            auto tagIt = params.find("tag");
            if (tagIt != params.end() && tagIt->value().is_string()) {
                runtime.rxTags.push_back(tagIt->value().as_string().c_str());
            }
        }
    }

    runtime.conveyor->addModule(module);
    return true;
}

std::any JsonReader::jsonToAny(const value& v) {
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

    // Arrays/objects are unsupported for now.
    return std::any{};
}
