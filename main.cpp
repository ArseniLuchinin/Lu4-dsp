#include <ConveyorOrchestrator.hpp>
#include <iostream>

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/support/date_time.hpp>

namespace logging = boost::log;
namespace expr = boost::log::expressions;

bool checkGPU();

/// ANSI-коды цветов для терминала
namespace colors {
    inline const char* reset   = "\033[0m";
    inline const char* red     = "\033[31m";
    inline const char* green   = "\033[32m";
    inline const char* yellow  = "\033[33m";
    inline const char* blue    = "\033[34m";
    inline const char* cyan    = "\033[36m";
    inline const char* gray    = "\033[90m";
}

void init_logging()
{
    logging::add_console_log(
        std::cout,
        logging::keywords::format =
            (
                expr::stream
                    << "[" << expr::attr<unsigned int>("LineID") << "] "
                    << "[" << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%H:%M:%S.%f") << "] "
                    << "["
                    << expr::if_(expr::attr<boost::log::trivial::severity_level>("Severity") == boost::log::trivial::error)
                       [
                           expr::stream << colors::red << "error" << colors::reset
                       ]
                    << expr::if_(expr::attr<boost::log::trivial::severity_level>("Severity") == boost::log::trivial::warning)
                       [
                           expr::stream << colors::yellow << "warning" << colors::reset
                       ]
                    << expr::if_(expr::attr<boost::log::trivial::severity_level>("Severity") == boost::log::trivial::info)
                       [
                           expr::stream << colors::green << "info" << colors::reset
                       ]
                    << expr::if_(expr::attr<boost::log::trivial::severity_level>("Severity") == boost::log::trivial::debug)
                       [
                           expr::stream << colors::cyan << "debug" << colors::reset
                       ]
                    << "] "
                    << "[" << expr::attr<std::string>("Channel") << "] "
                    << expr::smessage
            )
    );

    logging::add_common_attributes();
}


int32_t main(int argc, char const *argv[])
{
    if(not checkGPU()){
        return -1;
    }

    init_logging();
    const std::string configPath = (argc > 1) ? argv[1] : "/home/luchinin/my_source/Course_poject/Server/pipeline.json";

    ConveyorOrchestrator orchestrator(configPath);
    if (!orchestrator.load()) {
        std::cerr << "Failed to load config: " << configPath << std::endl;
        return 1;
    }

    if (!orchestrator.run()) {
        std::cerr << "Failed to run pipeline" << std::endl;
        return 1;
    }

    return 0;
}
