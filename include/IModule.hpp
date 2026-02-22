#ifndef IMODULE_H
#define IMODULE_H

#include <string>
#include <memory>
#include <IData.hpp>
#include <ModuleMetaData.hpp>

/*!
* @brief Интерфейс модуля
* @details Базовый интерфейс модуля, 
* который должен реализовываться каждый модуль
*/
class IModule {
public:
    virtual ~IModule() = default;
    
    /*!
    * @brief init инициализирует модуль из json 
    * @param json
    * @return true если модуль успешно инициализирован
    * @return false если есть ошибки
    */
    virtual bool init() = 0;

    /// @brief run запускает модуль
    virtual bool run() = 0;

    /*!
    * @brief setParam устанавливает параметр модуля
    * @param paramName имя параметра
    * @param value значение параметра
    */
    virtual void setParam(const std::string& paramName, const std::string& value) = 0;

    virtual bool setData(std::shared_ptr<IData> data) = 0;
    virtual std::shared_ptr<IData> getData() = 0;

    virtual ModuleMetaData getMetaData() = 0;

protected:
    ModuleMetaData m_metaData;
};

#endif // IMODULE_H