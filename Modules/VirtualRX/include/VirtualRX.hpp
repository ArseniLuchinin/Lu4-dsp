#ifndef VIRTUAL_RX_HPP
#define VIRTUAL_RX_HPP

#include <IModule.hpp>
#include <IVirtualRX.hpp>
#include <IData.hpp>
#include <any>
#include <memory>
#include <string>

/*! @brief Класс модуля ожидания виртуально переданных данных
 * @note Ожидает данные по тегу и передает их дальше по конвееру
 */
class VirtualRX : public IModule, public IVirtualRX {
public:
    VirtualRX();
    ~VirtualRX() override;

    bool init() override;
    bool run() override;

    void setParam(const std::string& paramName, const std::any& value) override;

    bool setData(std::shared_ptr<IData> data) override;
    std::shared_ptr<IData> getData() override;

private:
    std::shared_ptr<IData> m_data;
};

#endif // VIRTUAL_RX_HPP
