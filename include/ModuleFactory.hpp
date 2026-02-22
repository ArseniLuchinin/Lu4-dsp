#ifndef MODULE_FACTORY_H
#define MODULE_FACTORY_H

#include <memory>
#include <string>
#include <unordered_map>

#include <IModule.hpp>
#include <RedisClient.hpp>

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
    modulesPackage_t modulesPackage;
    RedisClient m_redis;
};

#endif // MODULE_FACTORY_H