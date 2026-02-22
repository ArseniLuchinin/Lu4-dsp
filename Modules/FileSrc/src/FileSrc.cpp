#include "FileSrc.hpp"
#include <fstream>
#include <iostream>
#include <cstring>
#include <filesystem>

#include <cuda_runtime.h>

#include <GpuFloatSignal.hpp>
#include <module.hpp>

namespace {
    constexpr size_t MAX_SIZE = 100'000;
}

IModule* createModule() {
    return new FileSrc();
}

// Вызывается после заполнения значений
bool FileSrc::init() {
    if(m_fileName.empty())  
        return false;
    
    if(not std::filesystem::exists(m_fileName)){
        std::cerr << "[FileSrc] Error: File not found: " << m_fileName << std::endl;
        return false;
    }

    const auto fileSize = std::filesystem::file_size(m_fileName);
    m_stepSize = (fileSize > MAX_SIZE) ? MAX_SIZE : fileSize;


    m_file = std::ifstream(m_fileName, std::ios::in | std::ios::binary);
    if (!m_file.is_open()) {
        std::cerr << "[FileSrc] Error: Failed to open file: " << m_fileName << std::endl;
        return false;
    }

    m_hostBuffer = new char[m_stepSize];

    return true;
}


bool FileSrc::run() {
    if(readFile())
        return true;
    return false;
}

void FileSrc::setParam(const std::string& paramName, const std::string& value) {
    if(paramName == "file name") {
        m_fileName = value;
        return;
    }

    if(paramName == "data type") {
        if(value == "float") 
            m_type = DataType::Float;
        return;
    }

    std::cerr << "Uncknow command" << std::endl;
}

bool FileSrc::setData(std::shared_ptr<IData> data) {
    return false;
}

std::shared_ptr<IData> FileSrc::getData() {
    return m_data;
}

bool FileSrc::readFile() {
    m_file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    if(m_file.is_open()) {
        std::shared_ptr<GpuFloatSignal> tryData = std::shared_ptr<GpuFloatSignal>(new GpuFloatSignal(m_stepSize));
        try{
            m_file.read(m_hostBuffer, m_stepSize);
        }
        catch(std::exception e){
            std::cout << e.what() << std::endl;
            return false;
        }
        if(m_file.fail()){
            return false;
        }

        tryData->setDataFromHost((float*)m_hostBuffer, m_file.gcount() / sizeof(float));
        m_data = tryData;
        return true;
    }

    return false;
}