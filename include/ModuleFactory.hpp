#ifndef MODULE_FACTORY_H
#define MODULE_FACTORY_H

#include <sw/redis++/redis++.h>

#include <IModule.hpp>

#include <memory>
#include <string>
#include <unordered_map>

class ModuleFactory {
    /// @brief Список модулей и директории с .so файлом и метаданными 
    using modulesPackage_t = std::unordered_map<std::string, std::string>;
public:
    /*!
     * @brief ModuleFactory инициализирует фабрику модулей из папки modulesDir
     * @param modulesDir
     **/
    ModuleFactory(const std::string& modulesDir);

    std::string findModule(const std::string& moduleName);
    IModule* createModule(const std::string& moduleName);

private:
    sw::redis::Redis m_redis;
    modulesPackage_t modulesPackage;
};

#endif // MODULE_FACTORY_H