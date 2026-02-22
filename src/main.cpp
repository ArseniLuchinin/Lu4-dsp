#include <Conveyor.hpp>
#include <ModuleFactory.hpp>

#include <CpuFloatSignal.hpp>
#include <iostream>

int32_t main(int argc, char const *argv[])
{
    ModuleFactory factory(".");

    Conveyor conveyor("Main conveyor");
    std::shared_ptr<IModule> module1 = std::shared_ptr<IModule>(factory.createModule("FileSrc"));
    module1->setParam("file name", "/home/luchinin/my_source/Course_poject/Server/signal_examples/sin_sognal.bin");
    module1->setParam("data type", "float");
    std::shared_ptr<IModule> module2 = std::shared_ptr<IModule>(factory.createModule("SumReduce"));

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
