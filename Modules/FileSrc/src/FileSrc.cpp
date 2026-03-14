#include "FileSrc.hpp"
#include <fstream>
#include <iostream>
#include <cstring>
#include <filesystem>

#include <cuda_runtime.h>

#include <GpuFloatSignal.hpp>
#include <GpuComplexSignal.hpp>
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
        if(valueStr == "complex")
            m_type = DataType::ComplexFloat;
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
        const size_t elementSize = (m_type == DataType::Float) ? sizeof(float) : sizeof(cuComplex);
        const size_t elementCount = m_stepSize / elementSize;
        if (elementCount == 0) {
            return false;
        }

        std::shared_ptr<IData> tryData;
        if (m_type == DataType::Float) {
            tryData = std::shared_ptr<GpuFloatSignal>(new GpuFloatSignal(elementCount));
        } else {
            tryData = std::shared_ptr<GpuComplexFloatSignal>(new GpuComplexFloatSignal(elementCount));
        }

        try{
            m_file.read(m_hostBuffer, m_stepSize);
        }
        catch(std::ios::failure e){
            if(m_file.eof()){
                if (m_type == DataType::Float) {
                    m_data = std::make_shared<GpuFloatSignal>(0);
                } else {
                    m_data = std::make_shared<GpuComplexFloatSignal>(0);
                }
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

        const auto readedSize = m_file.gcount() / elementSize;
        INFO << "Readed size: " << readedSize << " from file: " << m_fileName << std::endl;
        if (m_type == DataType::Float) {
            std::dynamic_pointer_cast<GpuFloatSignal>(tryData)->setDataFromHost(
                reinterpret_cast<float*>(m_hostBuffer),
                readedSize
            );
        } else {
            std::dynamic_pointer_cast<GpuComplexFloatSignal>(tryData)->setDataFromHost(
                reinterpret_cast<cuComplex*>(m_hostBuffer),
                readedSize
            );
        }
        m_data = tryData;
        return true;
    }

    return false;
}


FileSrc::~FileSrc(){
}
