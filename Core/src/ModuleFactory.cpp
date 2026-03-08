#include <ModuleFactory.hpp>
#include <dlfcn.h>
#include <cstdlib>
#include <iostream>
#include <logger.hpp>


namespace {
    sw::redis::ConnectionOptions createConnection() {
    sw::redis::ConnectionOptions opts;
    opts.host = "127.0.0.1";
    opts.port = 6379;
    opts.password = std::getenv("REDISCLI_AUTH");
        return opts;
    }
}

ModuleFactory::ModuleFactory(const std::string& modulesDir) : 
    m_redis(createConnection()), 
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
    const auto redisName = moduleName + "-module";
    const auto libPath = m_redis.get(redisName);
    if(libPath)
        return *libPath;
    
    return "";
}
