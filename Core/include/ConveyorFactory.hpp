#ifndef CONVEYOR_FACTORY_HPP
#define CONVEYOR_FACTORY_HPP

#include <Conveyor.hpp>
#include <ModuleFactory.hpp>
#include <ModuleMetaData.hpp>
#include <logger.hpp>

#include <boost/json.hpp>

#include <any>
#include <memory>
#include <string>

class ConveyorFactory {
public:
    explicit ConveyorFactory(ModuleFactory& moduleFactory);

    std::shared_ptr<Conveyor> createFromJsonObject(
        const boost::json::object& conveyorObj
    );
    std::shared_ptr<Conveyor> createFromJsonString(
        const std::string& jsonStr
    );

private:
    bool buildModule(
        Conveyor& conveyor,
        const boost::json::object& moduleObj
    );

    static std::any jsonToAny(const boost::json::value& value);
    static std::any convertParamValue(
        const boost::json::value& value,
        const ModuleMethaDataReader& reader,
        const std::string& paramName
    );

    ModuleFactory& m_factory;
    src::severity_channel_logger<
        logging::trivial::severity_level
    > logger;
};

#endif // CONVEYOR_FACTORY_HPP
