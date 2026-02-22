#ifndef FILE_SRC_H
#define FILE_SRC_H

#include <IModule.hpp>
#include <GpuFloatSignal.hpp>
#include <fstream>
#include <memory>

class SumReduce : public IModule {
public:
    SumReduce() = default;
    ~SumReduce() = default;

    /// @brief Выделяет память для данных
    bool init() override;
    /// @brief Чиатет данные из файл
    bool run() override;

    void setParam(const std::string& paramName, const std::string& value) override;

    bool setData(std::shared_ptr<IData> data) override;
    std::shared_ptr<IData> getData() override;

    ModuleMetaData getMetaData() override {return m_metaData;}  

private:
    std::shared_ptr<GpuFloatSignal> m_data;
    ModuleMetaData m_metaData {"FileSrc", "FileSrc.so", "FileSrc.json"};

};

#endif