#ifndef I_VIRTUAL_RX_MODULE_HPP
#define I_VIRTUAL_RX_MODULE_HPP

#include <IModule.hpp>
#include <IData.hpp>
#include <string>

class VirtualRX {
public:
    VirtualRX();
    virtual ~VirtualRX();

    bool setTag(const std::string& tag);
    std::shared_ptr<IData> rxData();

protected:
    std::string m_tag;
};

#endif // I_VIRTUAL_RX_MODULE_HPP
