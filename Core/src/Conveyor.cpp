// src/Conveyor.cpp
#include <Conveyor.hpp>
#include <iostream> // Для базового логирования
#include <stdexcept> // Для исключений

#include <CpuFloatSignal.hpp>


Conveyor::Conveyor(const std::string& name)
    : m_conveyorName(name)
    , m_isInitialized(false)
    , logger(boost::log::keywords::channel = name) {
    INFO << "Conveyor '" << m_conveyorName << "' created." << std::endl;
}

const std::vector<std::shared_ptr<IModule>>& Conveyor::getModules() const {
    return m_modules;
}

std::string Conveyor::getName() const {
    return m_conveyorName;
}

bool Conveyor::getIsInitialized() const {
    return m_isInitialized;
}

void Conveyor::addModule(std::shared_ptr<IModule> module) {
    if (module) {
        m_modules.push_back(module);
        INFO << "Module added to conveyor '" << m_conveyorName << "'. Total modules: " << m_modules.size() << std::endl;
    } else {
        ERROR << "Attempted to add a null module to conveyor '" << m_conveyorName << "'." << std::endl;
    }
}

void Conveyor::removeModule(size_t index) {
    if (index < m_modules.size()) {
        INFO << "Removing module at index " << index << " from conveyor '" << m_conveyorName << "'." << std::endl;
        m_modules.erase(m_modules.begin() + index);
    } else {
        ERROR << "Error: Index " << index << " out of bounds for conveyor '" << m_conveyorName << "'." << std::endl;
    }
}


bool Conveyor::init() {
    if(m_modules.empty())
        return false;

    for(auto& module : m_modules){
        if(not module->init()){
            ERROR << "Failed to initialize module" << std::endl;
            return false;
        }
    }

    m_isInitialized = true;
    return true; 
}

bool Conveyor::run() {
    auto module = m_modules.front();
    if(not module->run()){
        ERROR << "Fail to run: " << module->getMetaData().moduleName << std::endl;
        return false;
    }

    auto data = module->getData();
    if(data->size() == 0){
        INFO << "finished" << std::endl;
        return false;
    }

    for(size_t i = 1; i < m_modules.size(); ++i){
        module = m_modules[i];

        if(not data->isValid()){
            ERROR << "Invalid data from: " << module->getMetaData().moduleName << std::endl;
            return false;
        }

        if (module) {
            if(not module->setData(data)){
                ERROR << module->getMetaData().moduleName << "Con't use data tupe: " << data->getDataName() << std::endl;
                return false;
            }
            
            if(not module->run()){
                ERROR << "Fail to run: " << module->getMetaData().moduleName << std::endl;
                return false;
            }

            INFO << module->getMetaData().moduleName << " Success run" << std::endl;
            data = module->getData();
        }
    }
    return true;
}