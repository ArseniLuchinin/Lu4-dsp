#include <Conveyor.hpp>
#include <ModuleFactory.hpp>

#include <CpuFloatSignal.hpp>
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

    const int32_t fftSize = 4096;
    const int32_t sampleFreq = 2'097'152;
    init_logging();
    ModuleFactory factory(".");

    Conveyor conveyor("Main conveyor");
    std::shared_ptr<IModule> srcModule = std::shared_ptr<IModule>(factory.createModule("FileSrc"));
    srcModule->setParam("file name", std::string("/home/luchinin/my_source/Course_poject/Server/signal_examples/am_signal.bin"));
    srcModule->setParam("data type", std::string("float"));
    srcModule->setParam("max size", size_t(std::pow(2, 25)));

    std::shared_ptr<IModule> firModule = std::shared_ptr<IModule>(factory.createModule("FIR-filter"));
    firModule->setParam("sample rate", sampleFreq);
    firModule->setParam("filter order", 129);
    firModule->setParam("block size", 2048);
    firModule->setParam("low cutoff", float(51'000.f));
    firModule->setParam("high cutoff", float(54'000.f));
    
    std::shared_ptr<IModule> mobule1 = std::shared_ptr<IModule>(factory.createModule("SumReduce"));

    std::shared_ptr<IModule> fftMobule = std::shared_ptr<IModule>(factory.createModule("FFT"));
    fftMobule->setParam("fft size", fftSize);
    fftMobule->setParam("overlap size", fftSize / 2);

    std::shared_ptr<IModule> magnitureModyule = std::shared_ptr<IModule>(factory.createModule("CS2AS"));

    std::shared_ptr<IModule> spectrogramm = std::shared_ptr<IModule>(factory.createModule("SpectrogramPlot"));
    spectrogramm->setParam("show", true);
    spectrogramm->setParam("sample rate", sampleFreq);
    spectrogramm->setParam("fft size", fftSize);
    spectrogramm->setParam("save path", std::string("/home/luchinin/my_source/Course_poject/Server/signal_examples/spectrogramm"));


    conveyor.addModule(srcModule);
    //conveyor.addModule(firModule);
    conveyor.addModule(fftMobule);
    conveyor.addModule(magnitureModyule);
    conveyor.addModule(spectrogramm);


    if(not conveyor.init()){
        std::cerr << "Error init" << std::endl;
        return 1;
    }

    while(conveyor.run()){}
   

    std::cout << "Conveyor " << conveyor.getName() << "is stop" << std::endl;
    return 0;
}
