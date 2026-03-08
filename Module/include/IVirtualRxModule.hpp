#ifndef VIRTUALTRANSMITTER_CPP
#define VIRTUALTRANSMITTER_CPP

#include <IModule.hpp>
#include <IData.hpp>

#include <map>

class IVirtualRxModule : public IModule {
public:
    IVirtualRxModule();
    ~IVirtualRxModule();

    bool setTag(const std::string& tag);
    std::shared_ptr<IData> rxData();
};

#endif // VIRTUALTRANSMITTER_CPP