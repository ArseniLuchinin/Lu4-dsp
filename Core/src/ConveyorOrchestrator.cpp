#include <ConveyorOrchestrator.hpp>

#include <Variables.hpp>

#include <boost/system/error_code.hpp>

#include <chrono>
#include <fstream>
#include <sstream>

ConveyorOrchestrator::ConveyorOrchestrator(const std::string& configPath, const std::string& modulesDir)
    : m_configPath(configPath)
    , m_moduleFactory(modulesDir)
    , m_conveyorFactory(m_moduleFactory)
    , logger(boost::log::keywords::channel = "ConveyorOrchestrator")
{}

bool ConveyorOrchestrator::load() {
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

bool ConveyorOrchestrator::run() {
    if (!m_loaded) {
        ERROR << "ConveyorOrchestrator::run: config not loaded." << std::endl;
        return false;
    }

    const auto* rootObj = m_root.if_object();
    if (!rootObj) {
        ERROR << "ConveyorOrchestrator::run: root is not an object." << std::endl;
        return false;
    }

    if (!loadVariables(*rootObj)) {
        return false;
    }

    if (!buildConveyors()) {
        return false;
    }

    for (auto& config : m_conveyorConfigs) {
        m_runtimes.emplace_back();
    }

    for (size_t i = 0; i < m_runtimes.size(); ++i) {
        auto& runtime = m_runtimes[i];
        const auto& config = m_conveyorConfigs[i];

        runtime.thread = std::thread([this, &runtime, &config]() {
            const auto start = std::chrono::steady_clock::now();

            try {
                runtime.conveyor = m_conveyorFactory.createFromJsonObject(config);
            } catch (const std::exception& ex) {
                ERROR << "Failed to build conveyor: " << ex.what() << std::endl;
                return;
            }

            std::string conveyorName = runtime.conveyor->getName();

            if (!runtime.conveyor->init()) {
                ERROR << "Error init conveyor: " << conveyorName << std::endl;
                runtime.conveyor.reset();
                return;
            }

            while (runtime.conveyor->run()) {
            }

            runtime.name = conveyorName;
            runtime.conveyor.reset();
            const auto end = std::chrono::steady_clock::now();
            runtime.elapsedSeconds =
                std::chrono::duration<double>(end - start).count();
        });
    }

    for (auto& runtime : m_runtimes) {
        if (runtime.thread.joinable()) {
            runtime.thread.join();
        }
    }

    for (const auto& runtime : m_runtimes) {
        const std::string name = runtime.name.empty()
            ? std::string("<destroyed>")
            : runtime.name;
        INFO << "Conveyor '" << name << "' total time: "
             << runtime.elapsedSeconds << " s" << std::endl;
    }

    return true;
}

bool ConveyorOrchestrator::loadVariables(const boost::json::object& root) {
    auto it = root.find("variables");
    if (it == root.end()) {
        return true;
    }

    if (!it->value().is_string()) {
        ERROR << "'variables' must be a string path." << std::endl;
        return false;
    }

    const std::string varsPath = it->value().as_string().c_str();
    if (!Variables::instance().load(varsPath)) {
        ERROR << "Failed to load variables: " << varsPath << std::endl;
        return false;
    }

    return true;
}

bool ConveyorOrchestrator::buildConveyors() {
    const auto* rootObj = m_root.if_object();
    if (!rootObj) {
        ERROR << "ConveyorOrchestrator::buildConveyors: root is not object." << std::endl;
        return false;
    }

    auto it = rootObj->find("conveyors");
    if (it == rootObj->end() || !it->value().is_array()) {
        ERROR << "'conveyors' must be an array." << std::endl;
        return false;
    }

    const auto& conveyors = it->value().as_array();
    for (const auto& item : conveyors) {
        const auto* obj = item.if_object();
        if (!obj) {
            ERROR << "Conveyor entry is not an object." << std::endl;
            return false;
        }

        m_conveyorConfigs.push_back({*obj});
    }

    return true;
}
