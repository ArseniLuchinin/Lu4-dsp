#include "FileWriter.hpp"
#include <VariablesResolve.hpp>
#include <IGpuSignalData.hpp>
#include <module.hpp>
#include <cstdint>
#include <vector>
#include <cuda_runtime.h>
#include <sys/stat.h>

IModule* createModule() {
    return new FileWriter();
}

FileWriter::FileWriter()
    : IModule({"FileWriter", "", ""})
{}

FileWriter::~FileWriter() {
    DEBUG << "FileWriter::~FileWriter()" << std::endl;
    if (m_out.is_open()) {
        m_out.close();
    }
    DEBUG << "FileWriter::~FileWriter()" << std::endl;
}

bool FileWriter::init() {
    if (m_fileName.empty()) {
        ERROR << "FileWriter::init failed: file name is empty." << std::endl;
        return false;
    }

    struct stat buffer;
    bool fileExists = (stat(m_fileName.c_str(), &buffer) == 0);
    
    if (fileExists && !m_overwrite) {
        ERROR << "FileWriter::init failed: file already exists and overwrite is disabled: " << m_fileName << std::endl;
        return false;
    }

    if (fileExists && m_overwrite) {
        if (std::remove(m_fileName.c_str()) != 0) {
            ERROR << "FileWriter::init failed: can't remove existing file: " << m_fileName << std::endl;
            return false;
        }
    }

    m_out.open(m_fileName, std::ios::binary | std::ios::trunc);
    if (!m_out.is_open()) {
        ERROR << "FileWriter::init failed: can't open file: " << m_fileName << std::endl;
        return false;
    }

    return true;
}

bool FileWriter::setData(std::shared_ptr<IData> data) {
    m_inDataGpu = std::dynamic_pointer_cast<IGpuSignalData>(data);
    if (!m_inDataGpu) {
        ERROR << "FileWriter::setData failed: input data must be a GPU signal (IGpuSignalData)." << std::endl;
        return false;
    }

    if (!m_inDataGpu->isValid()) {
        ERROR << "FileWriter::setData failed: input data is invalid." << std::endl;
        return false;
    }

    m_outData = data;
    return true;
}

bool FileWriter::run() {
    if (!m_inDataGpu) {
        ERROR << "FileWriter::run failed: input data is null." << std::endl;
        return false;
    }

    if (!m_out.is_open()) {
        ERROR << "FileWriter::run failed: output file is not open." << std::endl;
        return false;
    }

    size_t elementSize = m_inDataGpu->elementSizeBytes();
    size_t logicalSize = m_inDataGpu->size();
    size_t byteSize = logicalSize * elementSize;

    if (byteSize == 0) {
        return true;
    }

    std::vector<uint8_t> hostBytes(byteSize);
    if (hostBytes.empty()) {
        ERROR << "FileWriter::run failed: failed to allocate host buffer." << std::endl;
        return false;
    }

    cudaError_t copyErr = cudaMemcpy(
        hostBytes.data(),
        m_inDataGpu->deviceDataRaw(),
        byteSize,
        cudaMemcpyDeviceToHost
    );
    if (copyErr != cudaSuccess) {
        ERROR << "FileWriter::run failed: cudaMemcpy D2H failed: "
              << cudaGetErrorString(copyErr) << std::endl;
        return false;
    }

    m_out.write(reinterpret_cast<const char*>(hostBytes.data()), 
                static_cast<std::streamsize>(byteSize));
    if (!m_out.good()) {
        ERROR << "FileWriter::run failed: write error for file: " << m_fileName << std::endl;
        return false;
    }
    m_out.flush();
    if (!m_out.good()) {
        ERROR << "FileWriter::run failed: flush error for file: " << m_fileName << std::endl;
        return false;
    }

    return true;
}

void FileWriter::setParam(const std::string& paramName, const std::any& value) {
    if (paramName == "file name") {
        m_fileName = std::any_cast<std::string>(value);
        return;
    }

    if (paramName == "overwrite") {
        m_overwrite = std::any_cast<bool>(value);
        return;
    }

    ERROR << "FileWriter::setParam unknown parameter: " << paramName << std::endl;
}

std::shared_ptr<IData> FileWriter::getData() {
    return m_outData;
}