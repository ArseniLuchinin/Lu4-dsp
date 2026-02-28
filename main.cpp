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

int count;
cudaError_t err = cudaGetDeviceCount(&count);
printf("count=%d err=%s\n", count, cudaGetErrorString(err));
    Conveyor conveyor("Main conveyor");
    std::shared_ptr<IModule> srcModule = std::shared_ptr<IModule>(factory.createModule("FileSrc"));
    srcModule->setParam("file name", std::string("/home/luchinin/my_source/Course_poject/Server/signal_examples/sin_signal.bin"));
    srcModule->setParam("data type", std::string("float"));

    std::shared_ptr<IModule> module1 = std::shared_ptr<IModule>(factory.createModule("FFT"));
    std::shared_ptr<IModule> module2 = std::shared_ptr<IModule>(factory.createModule("CS2AS"));

    std::shared_ptr<IModule> viewModule = std::shared_ptr<IModule>(factory.createModule("SignalPlot"));
    viewModule->setParam("sample rate", 20000);
    viewModule->setParam("show", true);
    viewModule->setParam("save path", std::string("/home/luchinin/my_source/Course_poject/Server/signal_examples/sin_signal.png"));


    conveyor.addModule(srcModule);
    conveyor.addModule(module1);
    conveyor.addModule(module2);
    conveyor.addModule(viewModule);

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
