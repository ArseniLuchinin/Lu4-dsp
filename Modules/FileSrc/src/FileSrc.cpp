#include "FileSrc.hpp"
#include <fstream>
#include <iostream>
#include <cstring>
#include <filesystem>

#include <cuda_runtime.h>

#include <GpuFloatSignal.hpp>
#include <module.hpp>

IModule* createModule() {
    return new FileSrc();
}

// Вызывается после заполнения значений
bool FileSrc::init() {
    if(m_fileName.empty())  
        return false;
    
    if(not std::filesystem::exists(m_fileName)){
        ERROR << "File not found: " << m_fileName << std::endl;
        return false;
    }

    const auto fileSize = std::filesystem::file_size(m_fileName);
    m_stepSize = (fileSize > m_maxSize) ? m_maxSize : fileSize;


    m_file = std::ifstream(m_fileName, std::ios::in | std::ios::binary);
    if (!m_file.is_open()) {
        ERROR << "Failed to open file: " << m_fileName << std::endl;
        return false;
    }

    INFO << "Successfully opened file: " << m_fileName << std::endl;

    m_hostBuffer = new char[m_stepSize];

    return true;
}


bool FileSrc::run() {
    if(readFile())
        return true;
    return false;
}

void FileSrc::setParam(const std::string& paramName, const std::any& value) {
    if(paramName == "file name") {
        m_fileName = std::any_cast<std::string>(value);
        return;
    }

    if(paramName == "max size") {
        m_maxSize = std::any_cast<size_t>(value);
        return;
    }

    if(paramName == "data type") {
        auto valueStr = std::any_cast<std::string>(value);
        if(valueStr == "float") 
            m_type = DataType::Float;
        return;
    }
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
        std::shared_ptr<GpuFloatSignal> tryData = std::shared_ptr<GpuFloatSignal>(new GpuFloatSignal(m_stepSize/sizeof(float)));
        if(tryData->size() <= 0) {
            return false;
        }

        try{
            m_file.read(m_hostBuffer, m_stepSize);
        }
        catch(std::ios::failure e){
            if(m_file.eof()){
                m_data = std::make_shared<GpuFloatSignal>(0);
                return true;
            }

            ERROR << e.what() << std::endl;
            return false;
        }

        if(m_file.fail() or not m_file.good()){
            return false;
        }

        if(m_hostBuffer == nullptr)
            return false;

        const auto readedSize = m_file.gcount() / sizeof(float);
        INFO << "Readed size: " << readedSize << " from file: " << m_fileName << std::endl;
        tryData->setDataFromHost((float*)m_hostBuffer, readedSize);
        m_data = tryData;
        return true;
    }

    return false;
}


FileSrc::~FileSrc(){
}