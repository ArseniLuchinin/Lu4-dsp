#ifndef FILE_WRITER_HPP
#define FILE_WRITER_HPP

#include <IModule.hpp>
#include <IGpuSignalData.hpp>

#include <fstream>
#include <memory>
#include <string>

class FileWriter : public IModule {
public:
    FileWriter();
    ~FileWriter() override;

    bool init() override;
    bool run() override;

    void setParam(const std::string& paramName, const std::any& value) override;
    bool setData(std::shared_ptr<IData> data) override;
    std::shared_ptr<IData> getData() override;

private:
    std::string m_fileName;
    bool m_overwrite = false;

    std::ofstream m_out;

    std::shared_ptr<IGpuSignalData> m_inDataGpu;
    std::shared_ptr<IData> m_outData;
};

#endif // FILE_WRITER_HPP