#ifndef CONVEYOR_ORCHESTRATOR_HPP
#define CONVEYOR_ORCHESTRATOR_HPP

#include <ConveyorFactory.hpp>
#include <logger.hpp>

#include <boost/json.hpp>

#include <memory>
#include <string>
#include <thread>
#include <vector>

class ConveyorOrchestrator {
public:
    explicit ConveyorOrchestrator(const std::string& configPath);

    bool load();
    bool run();

private:
    struct Runtime {
        ConveyorFactory::BuildResult build;
        std::thread thread;
        double elapsedSeconds = 0.0;
    };

    bool buildConveyors();
    bool loadVariables(const boost::json::object& root);

    std::string m_configPath;
    boost::json::value m_root;
    bool m_loaded = false;

    ModuleFactory m_moduleFactory;
    ConveyorFactory m_conveyorFactory;
    std::vector<Runtime> m_runtimes;

    src::severity_channel_logger<
        logging::trivial::severity_level
    > logger;
};

#endif // CONVEYOR_ORCHESTRATOR_HPP
