#ifndef FILE_WRITER_HPP
#define FILE_WRITER_HPP

#include <IGpuSignalData.hpp>
#include <IModule.hpp>

#include <fstream>
#include <memory>
#include <string>
#include <vector>

class FileWriter : public IModule {
public:
  FileWriter();
  ~FileWriter() override;

  bool init() override;
  bool run() override;

  void setParam(const std::string &paramName, const std::any &value) override;
  bool setData(std::shared_ptr<IData> data) override;
  std::shared_ptr<IData> getData() override;

private:
  std::string m_fileName;
  bool m_overwrite = false;
  size_t m_bufferSize = 8 * 1024 * 1024;

  std::ofstream m_out;
  std::vector<char> m_streamBuf;

  std::shared_ptr<IGpuSignalData> m_inDataGpu;
  std::shared_ptr<IData> m_outData;

  uint8_t *m_hostBuffer = nullptr;
  size_t m_hostBufferSize = 0;
};

#endif // FILE_WRITER_HPP
