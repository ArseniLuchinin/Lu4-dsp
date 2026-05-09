#include <TextFileWriter.hpp>
#include <VariablesResolve.hpp>

#include <module.hpp>

#include <cstdint>
#include <vector>

#include <cuda_runtime.h>

IModule *createModule() { return new TextFileWriter(); }

TextFileWriter::TextFileWriter()
    : IModule(
          {"TextFileWriter", "libTextFileWriter-module.so", "module.json"}) {}

TextFileWriter::~TextFileWriter() {
  if (m_out.is_open()) {
    m_out.flush();
    m_out.close();
  }
}

bool TextFileWriter::init() {
  if (m_fileName.empty()) {
    ERROR << "TextFileWriter::init failed: file name is empty." << std::endl;
    return false;
  }

  if (m_bufferSize > 0) {
    m_streamBuf.resize(m_bufferSize);
    m_out.rdbuf()->pubsetbuf(m_streamBuf.data(), m_bufferSize);
  }

  m_out.open(m_fileName, std::ios::binary | std::ios::trunc);
  if (!m_out.is_open()) {
    ERROR << "TextFileWriter::init failed: can't open file: " << m_fileName
          << std::endl;
    return false;
  }

  return true;
}

bool TextFileWriter::setData(std::shared_ptr<IData> data) {
  m_inData = std::dynamic_pointer_cast<GpuByteSignal>(data);
  if (!m_inData) {
    ERROR << "TextFileWriter::setData failed: input must be GpuByteSignal with "
             "bytes."
          << std::endl;
    return false;
  }

  if (!m_inData->isValid()) {
    ERROR << "TextFileWriter::setData failed: input data is invalid."
          << std::endl;
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
    ERROR << "TextFileWriter::run failed: output file is not open."
          << std::endl;
    return false;
  }

  std::vector<uint8_t> hostBytes(m_inData->size());
  if (!hostBytes.empty()) {
    const auto copyErr =
        cudaMemcpy(hostBytes.data(), m_inData->getDeviceData(),
                   hostBytes.size() * sizeof(uint8_t), cudaMemcpyDeviceToHost);
    if (copyErr != cudaSuccess) {
      ERROR << "TextFileWriter::run failed: cudaMemcpy D2H failed: "
            << cudaGetErrorString(copyErr) << std::endl;
      return false;
    }

    m_out.write(reinterpret_cast<const char *>(hostBytes.data()),
                static_cast<std::streamsize>(hostBytes.size()));
    if (!m_out.good()) {
      ERROR << "TextFileWriter::run failed: write error for file: "
            << m_fileName << std::endl;
      return false;
    }
    m_out.flush();
    if (!m_out.good()) {
      ERROR << "TextFileWriter::run failed: flush error for file: "
            << m_fileName << std::endl;
      return false;
    }
  }

  return true;
}

void TextFileWriter::setParam(const std::string &paramName,
                              const std::any &value) {
  if (paramName == "file name") {
    m_fileName = std::any_cast<std::string>(value);
    return;
  }

  if (paramName == "buffer size") {
    m_bufferSize = std::any_cast<size_t>(value);
    return;
  }

  ERROR << "TextFileWriter::setParam unknown parameter: " << paramName
        << std::endl;
}

std::shared_ptr<IData> TextFileWriter::getData() { return m_outData; }
