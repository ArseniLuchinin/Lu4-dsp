// src/Conveyor.cpp
#include <Conveyor.hpp>
#include <iostream> // Для базового логирования
#include <stdexcept> // Для исключений

#include <CpuFloatSignal.hpp>


Conveyor::Conveyor(const std::string& name)
    : m_conveyorName(name)
    , m_isInitialized(false)
    , logger(boost::log::keywords::channel = name) {
    std::cout << "Conveyor '" << m_conveyorName << "' created." << std::endl;
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
        std::cout << "Module added to conveyor '" << m_conveyorName << "'. Total modules: " << m_modules.size() << std::endl;
    } else {
        std::cerr << "Attempted to add a null module to conveyor '" << m_conveyorName << "'." << std::endl;
    }
}

void Conveyor::removeModule(size_t index) {
    if (index < m_modules.size()) {
        std::cout << "Removing module at index " << index << " from conveyor '" << m_conveyorName << "'." << std::endl;
        m_modules.erase(m_modules.begin() + index);
    } else {
        std::cerr << "Error: Index " << index << " out of bounds for conveyor '" << m_conveyorName << "'." << std::endl;
    }
}


bool Conveyor::init() {
    for(auto& module : m_modules){
        if(not module->init()){
            std::cerr << "[Conveyor: " << m_conveyorName << "] Failed to initialize module '" << module->getMetaData().moduleName << "'" << std::endl;
            return false;
        }
    }

    m_isInitialized = true;
    return true; 
}

bool Conveyor::run() {
    std::cout << "Conveyor '" << m_conveyorName << "' running payload." << std::endl;
    if(m_modules.empty())
        return false;

    auto module = m_modules.front();
    if(not module->run()){
        std::cerr << "Fail to run: " << module->getMetaData().moduleName << std::endl;
        return false;
    }

    auto data = module->getData();
    if(data->size() == 0){
        std::cout << "! finished" << std::endl;
        return false;
    }

    for(size_t i = 1; i < m_modules.size(); ++i){
        module = m_modules[i];

        if(not data->isValid()){
            std::cerr << "Invalid data from: " << module->getMetaData().moduleName << std::endl;
            return false;
        }

        if (module) {
            if(not module->setData(data)){
                std::cerr << module->getMetaData().moduleName << "Con't use data tupe: " << data->getDataName() << std::endl;
                return false;
            }
            
            if(not module->run()){
                ERROR << "Fail to run: " << module->getMetaData().moduleName << std::endl;
                return false;
            }

            std::cout << "!" << module->getMetaData().moduleName << " Success run" << std::endl;
            data = module->getData();
        }
    }
    return true;
}