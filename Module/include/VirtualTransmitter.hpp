#ifndef VIRTUALTRANSMITTER_CPP
#define VIRTUALTRANSMITTER_CPP

#include <map>
#include <memory>

#include <IData.hpp>

/*!
 * @brief Синглтон-класс виртуальной передачи данных
 * @details Отвечает за хранение и тэго и передачи указателей на данные
*/
class VirtualTransmitter {
    static std::map<std::string, std::shared_ptr<IData> > s_transmitter;
public:
    bool findTeg(std::string name);
    bool checkData(std::string name);

    std::shared_ptr<IData> rxData(const std::string& name);
    void txData(const std::shared_ptr<IData> data, const std::string& name);
};

#endif // VIRTUALTRANSMITTER_CPP