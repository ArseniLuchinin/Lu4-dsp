#ifndef FILE_SRC_H
#define FILE_SRC_H

#include <IModule.hpp>
#include <fstream>

enum DataType{
    Float,
    ComplexFloat
};

/*!
 * @brief Класс источник данных
 * @details Читает данные из файла и загружает в GPU
*/
class FileSrc : public IModule {
public:

    FileSrc() : IModule({"FileSrc",  "FileSrc", "FileSrc"}) {};
    ~FileSrc();

    /// @brief Выделяет память для данных
    virtual bool init() override;
    /// @brief Чиатет данные из файл
    virtual bool run() override;

    virtual void setParam(const std::string& paramName, const std::any& value) override;

    virtual bool setData(std::shared_ptr<IData> data) override;
    virtual std::shared_ptr<IData> getData() override;

    bool readFile();

protected:
    DataType m_type;

    size_t m_maxSize;
    std::string m_fileName;
    size_t m_stepSize = 0;
    std::ifstream m_file;

    std::shared_ptr<IData> m_data;
    char* m_hostBuffer;
};

#endif