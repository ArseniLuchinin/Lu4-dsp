#include <ModuleFactory.hpp>
#include <dlfcn.h>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <logger.hpp>

ModuleFactory::ModuleFactory(const std::string& modulesDir) :
    m_modulesDir(modulesDir),
    logger(boost::log::keywords::channel = "ModuleFactory") {
}

IModule* ModuleFactory::createModule(const std::string& moduleName) {
    const auto libPath = findModule(moduleName);
    if(libPath.empty())
        return nullptr;

    void* moduleHandle = dlopen(libPath.c_str(), RTLD_LAZY);
    if (!moduleHandle) {
        ERROR << dlerror() << std::endl;
        return nullptr;
    }
    INFO << "Module loaded: " << moduleName << std::endl;

    using createModule_t = IModule* (*)();
    createModule_t module = (createModule_t) dlsym(moduleHandle, "createModule");
    if (!module) {
        ERROR << dlclose(moduleHandle);
        return nullptr;
    }
    INFO << "Module created" << std::endl;

    return module();
}

std::string ModuleFactory::findModule(const std::string& moduleName) {
    const auto libPath = std::filesystem::path(m_modulesDir) / moduleName / ("lib" + moduleName + "-module.so");
    if (std::filesystem::exists(libPath)) {
        return libPath.string();
    }

    ERROR << "Module not found: " << moduleName << " at " << libPath.string() << std::endl;
    return "";
}
