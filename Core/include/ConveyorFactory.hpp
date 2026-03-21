#ifndef CONVEYOR_FACTORY_HPP
#define CONVEYOR_FACTORY_HPP

#include <Conveyor.hpp>
#include <ModuleFactory.hpp>
#include <logger.hpp>

#include <boost/json.hpp>

#include <any>
#include <memory>
#include <string>

class ConveyorFactory {
public:
    struct BuildResult {
        std::shared_ptr<Conveyor> conveyor;
        std::string name;
    };

    explicit ConveyorFactory(ModuleFactory& moduleFactory);

    BuildResult createFromJsonObject(const boost::json::object& conveyorObj);
    BuildResult createFromJsonString(const std::string& jsonStr);

private:
    bool buildModule(
        BuildResult& result,
        const boost::json::object& moduleObj
    );

    static std::any jsonToAny(const boost::json::value& value);

    ModuleFactory& m_factory;
    src::severity_channel_logger<
        logging::trivial::severity_level
    > logger;
};

#endif // CONVEYOR_FACTORY_HPP
