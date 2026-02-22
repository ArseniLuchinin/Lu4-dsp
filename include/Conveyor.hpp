#ifndef CONVEYOR_H
#define CONVEYOR_H

#include <string>
#include <vector>
#include <memory>
#include <IModule.hpp>

class Conveyor {
public:

    bool init();
    bool run();
    
    explicit Conveyor(const std::string& name);
    
    void addModule(std::shared_ptr<IModule> module);
    void removeModule(size_t index);
    
    const std::vector<std::shared_ptr<IModule>>& getModules() const;
    std::string getName() const;
    bool getIsInitialized() const;

private:
    std::string m_conveyorName;
    bool m_isInitialized;
    std::vector<std::shared_ptr<IModule>> m_modules;
};

#endif // CONVEYOR_H