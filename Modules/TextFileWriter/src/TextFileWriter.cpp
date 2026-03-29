#include <TextFileWriter.hpp>
#include <VariablesResolve.hpp>

#include <module.hpp>

#include <cstdint>

IModule* createModule() {
    return new TextFileWriter();
}

TextFileWriter::TextFileWriter()
    : IModule({"TextFileWriter", "", ""})
{}

TextFileWriter::~TextFileWriter() {
    if (m_out.is_open()) {
        m_out.close();
    }
}

bool TextFileWriter::init() {
    if (m_fileName.empty()) {
        ERROR << "TextFileWriter::init failed: file name is empty." << std::endl;
        return false;
    }

    m_out.open(m_fileName, std::ios::binary | std::ios::trunc);
    if (!m_out.is_open()) {
        ERROR << "TextFileWriter::init failed: can't open file: " << m_fileName << std::endl;
        return false;
    }

    return true;
}

bool TextFileWriter::setData(std::shared_ptr<IData> data) {
    m_inData = std::dynamic_pointer_cast<CpuByteSignal>(data);
    if (!m_inData) {
        ERROR << "TextFileWriter::setData failed: input must be CpuByteSignal with bytes." << std::endl;
        return false;
    }

    if (!m_inData->isValid()) {
        ERROR << "TextFileWriter::setData failed: input data is invalid." << std::endl;
        return false;
    }

    m_outData = m_inData;
    return true;
}

bool TextFileWriter::run() {
    if (!m_inData) {
        ERROR << "TextFileWriter::run failed: input data is null." << std::endl;
        return false;
    }

    if (!m_out.is_open()) {
        ERROR << "TextFileWriter::run failed: output file is not open." << std::endl;
        return false;
    }

    const auto& bytes = m_inData->bytes();
    if (!bytes.empty()) {
        m_out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!m_out.good()) {
            ERROR << "TextFileWriter::run failed: write error for file: " << m_fileName << std::endl;
            return false;
        }
        m_out.flush();
        if (!m_out.good()) {
            ERROR << "TextFileWriter::run failed: flush error for file: " << m_fileName << std::endl;
            return false;
        }
    }

    return true;
}

void TextFileWriter::setParam(const std::string& paramName, const std::any& value) {
    const std::any resolved = resolveParamValue(value);

    if (paramName == "file name") {
        m_fileName = std::any_cast<std::string>(resolved);
        return;
    }

    ERROR << "TextFileWriter::setParam unknown parameter: " << paramName << std::endl;
}

std::shared_ptr<IData> TextFileWriter::getData() {
    return m_outData;
}
