#include <Conveyor.hpp>
#include <ModuleFactory.hpp>

#include <CpuFloatSignal.hpp>
#include <iostream>
#include <thread>
#include <chrono>

#include <EmptyContainer.hpp>
#include <VirtualTransmitter.hpp>

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/support/date_time.hpp> 
#include <Variables.hpp>

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
    Variables::instance().load("variables.toml");
    if(not checkGPU()){
        return -1;
    }


    const int32_t sampleFreq = 2'097'152;
    init_logging();
    ModuleFactory factory(".");

    const std::string rxTag = "fft_out";

    Conveyor rxConveyor("RX conveyor");
    std::shared_ptr<IModule> virtRxModule = std::shared_ptr<IModule>(factory.createModule("VirtualRX"));
    virtRxModule->setParam("tag", rxTag);

    std::shared_ptr<IModule> magnitudeModule = std::shared_ptr<IModule>(factory.createModule("CS2AS"));

    std::shared_ptr<IModule> spectrogramm = std::shared_ptr<IModule>(factory.createModule("SpectrogramPlot"));
    spectrogramm->setParam("sample rate", sampleFreq);
    spectrogramm->setParam("fft size", std::string("$FFTSize"));
    spectrogramm->setParam("window size", std::string("$FFTSize"));
    spectrogramm->setParam("centered spectrum", true);
    spectrogramm->setParam("freq min", -sampleFreq / 2.0);
    spectrogramm->setParam("freq max", sampleFreq / 2.0);
    spectrogramm->setParam("save path", std::string("/home/luchinin/my_source/Course_poject/Server/signal_examples/spectrogramm"));

    rxConveyor.addModule(virtRxModule);
    rxConveyor.addModule(magnitudeModule);
    rxConveyor.addModule(spectrogramm);

    std::thread rxThread([&](){
        if(not rxConveyor.init()){
            std::cerr << "Error init RX conveyor" << std::endl;
            return 1;
        }
        while(rxConveyor.run()) {}
        return 0;
    });

    Conveyor conveyor("Main conveyor");
    std::shared_ptr<IModule> srcModule = std::shared_ptr<IModule>(factory.createModule("FileSrc"));
    srcModule->setParam("file name", std::string("/home/luchinin/my_source/Course_poject/Server/signal_examples/amc_signal.bin"));
    srcModule->setParam("data type", std::string("complex"));
    srcModule->setParam("max size", std::string("$MaxSize"));

    std::shared_ptr<IModule> fftMobule = std::shared_ptr<IModule>(factory.createModule("FFT"));
    fftMobule->setParam("fft size", std::string("$FFTSize"));

    std::shared_ptr<IModule> virtTxModule = std::shared_ptr<IModule>(factory.createModule("VirtualTX"));
    virtTxModule->setParam("tag", rxTag);


    conveyor.addModule(srcModule);
    conveyor.addModule(fftMobule);
    conveyor.addModule(virtTxModule);

    if(not conveyor.init()){
        std::cerr << "Error init" << std::endl;
        return 1;
    }

    auto allStart = std::chrono::steady_clock::now();
    std::cout << "===== Run Main conveyor =====" << std::endl;
    while(conveyor.run()){}

    {
        VirtualTransmitter transmitter;
        transmitter.txData(std::make_shared<EmptyContainer>(), rxTag);
    }

    if(rxThread.joinable()){
        rxThread.join();
    }

    auto allEnd = std::chrono::steady_clock::now();
    auto allS = std::chrono::duration<double>(allEnd - allStart).count();
    std::cout << "All modules total time: " << allS << " s" << std::endl;

    return 0;
}
