#ifndef TEXT_FILE_WRITER_HPP
#define TEXT_FILE_WRITER_HPP

#include <IModule.hpp>

#include <GpuByteSignal.hpp>

#include <fstream>
#include <memory>
#include <string>
#include <vector>

class TextFileWriter : public IModule {
public:
    TextFileWriter();
    ~TextFileWriter() override;

    bool init() override;
    bool run() override;

    void setParam(const std::string& paramName, const std::any& value) override;
    bool setData(std::shared_ptr<IData> data) override;
    std::shared_ptr<IData> getData() override;

private:
    std::string m_fileName;
    size_t m_bufferSize = 8 * 1024 * 1024;

    std::ofstream m_out;
    std::vector<char> m_streamBuf;

    std::shared_ptr<GpuByteSignal> m_inData;
    std::shared_ptr<IData> m_outData;
};

#endif // TEXT_FILE_WRITER_HPP
