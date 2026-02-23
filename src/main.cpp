#include <Conveyor.hpp>
#include <ModuleFactory.hpp>

#include <CpuFloatSignal.hpp>
#include <iostream>

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

namespace logging = boost::log;
namespace expr = boost::log::expressions;

void init_logging()
{
    logging::add_console_log(
        std::cout,
        logging::keywords::format =
            (
                expr::stream
                    << "[" << expr::attr<unsigned int>("LineID") << "] "
                    << "[" << expr::attr<std::string>("Channel") << "] "
                    << expr::smessage
            )
    );

    logging::add_common_attributes();
}

int32_t main(int argc, char const *argv[])
{
    init_logging();
    ModuleFactory factory(".");

    Conveyor conveyor("Main conveyor");
    std::shared_ptr<IModule> module1 = std::shared_ptr<IModule>(factory.createModule("FileSrc"));
    module1->setParam("file name", std::string("/home/luchinin/my_source/Course_poject/Server/signal_examples/sin_sognal.bin"));
    module1->setParam("data type", std::string("float"));
    std::shared_ptr<IModule> module2 = std::shared_ptr<IModule>(factory.createModule("SignalPlot"));
    module2->setParam("sample rate", 20000);
    module2->setParam("show", true);
    module2->setParam("save path", std::string("/home/luchinin/my_source/Course_poject/Server/signal_examples/sin_signal.png"));

    conveyor.addModule(module1);
    conveyor.addModule(module2);

    if(not conveyor.init()){
        std::cerr << "Error init" << std::endl;
        return 1;
    }

    if(not conveyor.run()){
        std::cout << "Error run" << std::endl;
        return -1;
    }

    std::cout << "Success" << std::endl;

    return 0;
}
