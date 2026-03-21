#include <JsonReader.hpp>
#include <iostream>

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/support/date_time.hpp> 

namespace logging = boost::log;
namespace expr = boost::log::expressions;

bool checkGPU();

void init_logging()
{
    logging::add_console_log(
        std::cout,
        logging::keywords::format =
            (
                expr::stream
                    << "[" << expr::attr<unsigned int>("LineID") << "] "
                    << "[" << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%H:%M:%S.%f") << "] "
                    << "[" << expr::attr<boost::log::trivial::severity_level>("Severity") << "] " 
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

    JsonReader reader(configPath);
    if (!reader.load()) {
        std::cerr << "Failed to load config: " << configPath << std::endl;
        return 1;
    }

    if (!reader.run()) {
        std::cerr << "Failed to run pipeline" << std::endl;
        return 1;
    }

    return 0;
}
