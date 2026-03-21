#ifndef JSON_READER_HPP
#define JSON_READER_HPP

#include <Conveyor.hpp>
#include <ModuleFactory.hpp>
#include <logger.hpp>

#include <boost/json.hpp>

#include <any>
#include <memory>
#include <string>
#include <thread>
#include <vector>

class JsonReader {
public:
    explicit JsonReader(const std::string& configPath);

    bool load();
    bool run();

private:
    struct ConveyorRuntime {
        std::shared_ptr<Conveyor> conveyor;
        std::string name;
        bool hasVirtualRx = false;
        std::vector<std::string> rxTags;
        std::thread thread;
        double elapsedSeconds = 0.0;
    };

    bool buildConveyors();
    bool buildConveyor(const boost::json::object& conveyorObj);
    bool buildModule(
        ConveyorRuntime& runtime,
        const boost::json::object& moduleObj
    );

    static std::any jsonToAny(const boost::json::value& value);

    std::string m_configPath;
    boost::json::value m_root;
    bool m_loaded = false;

    ModuleFactory m_factory;
    std::vector<ConveyorRuntime> m_conveyors;

    src::severity_channel_logger<
        logging::trivial::severity_level
    > logger;
};

#endif // JSON_READER_HPP
