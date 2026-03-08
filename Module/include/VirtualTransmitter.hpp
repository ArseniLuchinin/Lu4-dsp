#ifndef VIRTUAL_TRANSMITTER_HPP
#define VIRTUAL_TRANSMITTER_HPP

#include <map>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <string>

#include <IData.hpp>

/*!
 * @brief Класс виртуальной передачи данных
 * @details Использует общее статическое хранилище тегированных указателей
*/
class VirtualTransmitter {
public:
    VirtualTransmitter() = default;
    ~VirtualTransmitter() = default;

    bool findTeg(const std::string& name);
    bool checkData(const std::string& name);

    std::shared_ptr<IData> rxData(const std::string& name);
    std::shared_ptr<IData> waitRxData(const std::string& name);
    void txData(const std::shared_ptr<IData>& data, const std::string& name);

private:
    static std::condition_variable& getTagCvUnlocked(const std::string& name);

    static std::map<std::string, std::shared_ptr<IData> > s_transmitter;
    static std::map<std::string, std::condition_variable> s_tagEvents;
    static std::mutex s_mutex;
};

#endif // VIRTUAL_TRANSMITTER_HPP
