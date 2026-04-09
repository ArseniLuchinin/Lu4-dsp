#ifndef VIRTUAL_TRANSMITTER_HPP
#define VIRTUAL_TRANSMITTER_HPP

#include <map>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <string>
#include <atomic>

#include <IData.hpp>

/*!
 * @brief Слот широковещательной рассылки
 * @details Хранит данные для рассылки нескольким RX.
 * Данные удаляются (nullptr) когда все RX получили копию.
*/
struct BroadcastSlot {
    std::shared_ptr<IData> data;           ///< Текущие данные (nullptr если все RX получили)
    size_t expectedReceivers = 0;          ///< Количество RX (авто-подсчёт из registerRx)
    size_t deliveredCount = 0;             ///< Сколько RX уже получили данные
    size_t iteration = 0;                  ///< Номер текущей итерации рассылки
    bool txRegistered = false;             ///< Зарегистрирован ли TX (тег может иметь только один TX)
    std::condition_variable rxCv;          ///< CV для синхронизации TX/RX
};

/*!
 * @brief Класс виртуальной передачи данных
 * @details Реализует broadcaster: один TX отправляет, N RX получают копию данных.
 * TX блокируется только если предыдущая итерация ещё не завершена (не все RX получили).
 * Тег может принадлежать только одному TX — валидация в registerTx().
*/
class VirtualTransmitter {
public:
    VirtualTransmitter() = default;
    ~VirtualTransmitter() = default;

    /*! @brief Зарегистрировать TX для тега (валидация уникальности)
     *  @return false если тег уже занят другим TX
    */
    static bool registerTx(const std::string& name);

    /*! @brief Зарегистрировать RX для тега (авто-инкремент счётчика) */
    static void registerRx(const std::string& name);

    /*! @brief Проверить наличие данных по тегу (неблокирующая) */
    bool checkData(const std::string& name);

    /*! @brief Получить данные без ожидания (nullptr если нет) */
    std::shared_ptr<IData> rxData(const std::string& name);

    /*! @brief Получить данные с ожиданием (копия данных для каждого RX) */
    std::shared_ptr<IData> waitRxData(const std::string& name);

    /*! @brief Отправить данные (TX продолжает работу, не ждёт подтверждения) */
    void txData(const std::shared_ptr<IData>& data, const std::string& name);

    /*! @brief Установить таймаут ожидания (мс). По умолчанию 45000. */
    static void setTimeoutMs(int ms);

    /*! @brief Получить текущий таймаут (мс) */
    static int getTimeoutMs();

    /*! @brief Устаревший метод, используйте registerTx */
    [[deprecated("Use registerTx instead")]]
    bool findTeg(const std::string& name);

private:
    static std::map<std::string, BroadcastSlot> s_broadcastSlots;
    static std::map<std::string, size_t> s_receiverCounts;
    static std::mutex s_mutex;
    static std::atomic<int> s_timeoutMs;
};

#endif // VIRTUAL_TRANSMITTER_HPP
