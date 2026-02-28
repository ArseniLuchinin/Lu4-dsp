#ifndef IDATA_H
#define IDATA_H

#include <logger.hpp>

#include <string>

class IData {
// Правило: память выделяется только в конструкторе
public:
    IData() = delete;
    IData(const std::string & dataName) :
        m_dataName(dataName),
        logger(boost::log::keywords::channel = dataName){};

    virtual ~IData() = default;
    inline const std::string& getDataName() const {return m_dataName;};

    /// @brief Возвращает размер памяти в элементах
    virtual size_t size() const = 0;


    /*!
    * @brief Выделяет память на GPU
    * @param size - размер памяти
    * @return true - если память выделена, false - если нет
    * 
    * @note
    * Выделяет память на GPU
    * Единственный метод для выделения памяти!
    */
    virtual bool reserve(const size_t size) = 0;

    /*!
    * @brief Проверка можно ли использовать данные
    * @return true - если можно использовать данные, false - если нет
    */
    virtual bool isValid() const = 0;

protected:
    src::severity_channel_logger<
        logging::trivial::severity_level
    > logger;
private:
    std::string m_dataName;
};

#endif // IDATA_H