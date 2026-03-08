#ifndef VIRTUAL_TRANSMITTER_HPP
#define VIRTUAL_TRANSMITTER_HPP

#include <map>
#include <memory>
#include <mutex>
#include <string>

#include <IData.hpp>

/*!
 * @brief Синглтон-класс виртуальной передачи данных
 * @details Отвечает за хранение и тэго и передачи указателей на данные
*/
class VirtualTransmitter {
public:
    static VirtualTransmitter& instance();

    VirtualTransmitter(const VirtualTransmitter&) = delete;
    VirtualTransmitter& operator=(const VirtualTransmitter&) = delete;
    VirtualTransmitter(VirtualTransmitter&&) = delete;
    VirtualTransmitter& operator=(VirtualTransmitter&&) = delete;

    bool findTeg(const std::string& name);
    bool checkData(const std::string& name);

    std::shared_ptr<IData> rxData(const std::string& name);
    void txData(const std::shared_ptr<IData>& data, const std::string& name);

private:
    VirtualTransmitter() = default;
    ~VirtualTransmitter() = default;

    std::map<std::string, std::shared_ptr<IData> > m_transmitter;
    std::mutex m_mutex;
};

#endif // VIRTUAL_TRANSMITTER_HPP
