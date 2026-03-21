#include "FileSrc.hpp"
#include <VariablesResolve.hpp>
#include <fstream>
#include <iostream>
#include <cstring>
#include <filesystem>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include <cuda_runtime.h>

#include <GpuFloatSignal.hpp>
#include <GpuComplexSignal.hpp>
#include <EmptyContainer.hpp>
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

    m_fileSize = std::filesystem::file_size(m_fileName);
    m_stepSize = (m_fileSize > m_maxSize) ? m_maxSize : m_fileSize;

    m_fd = ::open(m_fileName.c_str(), O_RDONLY);
    if (m_fd < 0) {
        ERROR << "Failed to open file: " << m_fileName << std::endl;
        return false;
    }

    if (m_fileSize > 0) {
        void* mapped = ::mmap(nullptr, m_fileSize, PROT_READ, MAP_PRIVATE, m_fd, 0);
        if (mapped == MAP_FAILED) {
            ERROR << "Failed to mmap file: " << m_fileName << std::endl;
            ::close(m_fd);
            m_fd = -1;
            return false;
        }
        m_mmapPtr = reinterpret_cast<uint8_t*>(mapped);
    }

    INFO << "Successfully opened file: " << m_fileName << std::endl;

    return true;
}


bool FileSrc::run() {
    if(readFile())
        return true;
    return false;
}

void FileSrc::setParam(const std::string& paramName, const std::any& value) {
    const std::any resolved = resolveParamValue(value);
    if(paramName == "file name") {
        m_fileName = std::any_cast<std::string>(resolved);
        return;
    }

    if(paramName == "max size") {
        m_maxSize = std::any_cast<size_t>(resolved);
        return;
    }

    if(paramName == "data type") {
        auto valueStr = std::any_cast<std::string>(resolved);
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
    if (m_fd < 0) {
        return false;
    }

    if (m_offset >= m_fileSize) {
        m_data = std::make_shared<EmptyContainer>();
        return true;
    }

    const size_t elementSize = (m_type == DataType::Float) ? sizeof(float) : sizeof(cuComplex);
    const size_t remaining = m_fileSize - m_offset;
    const size_t readBytes = (remaining > m_stepSize) ? m_stepSize : remaining;
    const size_t elementCount = readBytes / elementSize;
    if (elementCount == 0) {
        return false;
    }

    std::shared_ptr<IData> tryData;
    if (m_type == DataType::Float) {
        tryData = std::shared_ptr<GpuFloatSignal>(new GpuFloatSignal(elementCount));
    } else {
        tryData = std::shared_ptr<GpuComplexFloatSignal>(new GpuComplexFloatSignal(elementCount));
    }

    uint8_t* srcPtr = m_mmapPtr + m_offset;
    INFO << "Readed size: " << elementCount << " from file: " << m_fileName << std::endl;
    if (m_type == DataType::Float) {
        std::dynamic_pointer_cast<GpuFloatSignal>(tryData)->setDataFromHost(
            reinterpret_cast<float*>(srcPtr),
            elementCount
        );
    } else {
        std::dynamic_pointer_cast<GpuComplexFloatSignal>(tryData)->setDataFromHost(
            reinterpret_cast<cuComplex*>(srcPtr),
            elementCount
        );
    }

    m_offset += readBytes;
    m_data = tryData;
    return true;
}


FileSrc::~FileSrc(){
    if (m_mmapPtr != nullptr && m_fileSize > 0) {
        ::munmap(m_mmapPtr, m_fileSize);
        m_mmapPtr = nullptr;
    }
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}
