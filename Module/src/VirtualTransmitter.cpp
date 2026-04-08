#include <VirtualTransmitter.hpp>
#include <EmptyContainer.hpp>

#include <iostream>
#include <chrono>

std::map<std::string, BroadcastSlot> VirtualTransmitter::s_broadcastSlots;
std::map<std::string, size_t> VirtualTransmitter::s_receiverCounts;
std::mutex VirtualTransmitter::s_mutex;
std::atomic<int> VirtualTransmitter::s_timeoutMs{45000};  // 45 секунд по умолчанию

// ============================================================================
// Регистрация TX/RX
// ============================================================================

bool VirtualTransmitter::registerTx(const std::string& name, const std::string& txModuleName) {
    std::lock_guard<std::mutex> lock(s_mutex);

    auto& slot = s_broadcastSlots[name];

    // Если тег уже зарегистрирован другим TX — ошибка
    if (!slot.txOwner.empty() && slot.txOwner != txModuleName) {
        std::cerr << "VirtualTransmitter::registerTx failed: tag='" << name
                  << "' already registered by TX='" << slot.txOwner
                  << "', conflict with '" << txModuleName << "'." << std::endl;
        return false;
    }

    slot.txOwner = txModuleName;
    std::cout << "[DEBUG] VirtualTransmitter::registerTx tag='" << name
              << "' owned by '" << txModuleName << "'." << std::endl;
    return true;
}

void VirtualTransmitter::registerRx(const std::string& name) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_receiverCounts[name]++;

    auto& slot = s_broadcastSlots[name];
    slot.expectedReceivers = s_receiverCounts[name];

    std::cout << "[DEBUG] VirtualTransmitter::registerRx tag='" << name
              << "', receivers=" << s_receiverCounts[name] << std::endl;
}

// ============================================================================
// Таймаут
// ============================================================================

void VirtualTransmitter::setTimeoutMs(int ms) {
    s_timeoutMs.store(ms);
}

int VirtualTransmitter::getTimeoutMs() {
    return s_timeoutMs.load();
}

// ============================================================================
// Передача данных
// ============================================================================

void VirtualTransmitter::txData(const std::shared_ptr<IData>& data, const std::string& name) {
    std::unique_lock<std::mutex> lock(s_mutex);
    auto& slot = s_broadcastSlots[name];

    // Инициализация если первый раз (обратная совместимость)
    if (slot.expectedReceivers == 0) {
        slot.expectedReceivers = 1;
    }

    // Блокируемся ТОЛЬКО если предыдущие данные ещё не все получили
    if (slot.data != nullptr && slot.deliveredCount < slot.expectedReceivers) {
        bool delivered = slot.rxCv.wait_for(lock,
            std::chrono::milliseconds(s_timeoutMs.load()),
            [&]() { return slot.deliveredCount >= slot.expectedReceivers; });

        if (!delivered) {
            std::cerr << "VirtualTransmitter::txData timeout: tag='" << name
                      << "', delivered " << slot.deliveredCount
                      << "/" << slot.expectedReceivers << " in "
                      << (s_timeoutMs.load() / 1000) << "s." << std::endl;
            std::exit(1);
        }
    }

    // Записываем новые данные
    slot.data = data;
    slot.deliveredCount = 0;
    slot.iteration++;

    std::cout << "[INFO] VirtualTransmitter::txData published tag='" << name
              << "', iteration " << slot.iteration << std::endl;

    slot.rxCv.notify_all();
    // TX продолжает работу (не ждёт здесь)
}

std::shared_ptr<IData> VirtualTransmitter::waitRxData(const std::string& name) {
    std::unique_lock<std::mutex> lock(s_mutex);
    auto& slot = s_broadcastSlots[name];

    // Инициализация если первый раз
    if (slot.expectedReceivers == 0) {
        slot.expectedReceivers = 1;
    }

    const size_t currentIteration = slot.iteration;

    bool received = slot.rxCv.wait_for(lock,
        std::chrono::milliseconds(s_timeoutMs.load()),
        [&]() {
            return slot.data != nullptr && slot.iteration >= currentIteration;
        });

    if (!received) {
        std::cerr << "VirtualTransmitter::waitRxData timeout: tag='" << name
                  << "', waited " << (s_timeoutMs.load() / 1000)
                  << "s for iteration " << currentIteration << std::endl;
        std::exit(1);
    }

    // Копируем данные (каждый RX получает свою копию)
    const auto dataCopy = slot.data->copy();
    slot.deliveredCount++;

    std::cout << "[INFO] VirtualTransmitter::waitRxData received tag='" << name
              << "', iteration " << slot.iteration
              << ", delivered " << slot.deliveredCount
              << "/" << slot.expectedReceivers << std::endl;

    // Если все получили — очищаем слот и будим TX (если ждёт)
    if (slot.deliveredCount >= slot.expectedReceivers) {
        slot.data = nullptr;
        slot.rxCv.notify_all();
    }

    return dataCopy;
}

std::shared_ptr<IData> VirtualTransmitter::rxData(const std::string& name) {
    std::lock_guard<std::mutex> lock(s_mutex);
    auto& slot = s_broadcastSlots[name];

    // Инициализация если первый раз
    if (slot.expectedReceivers == 0) {
        slot.expectedReceivers = 1;
    }

    if (slot.data == nullptr || slot.deliveredCount >= slot.expectedReceivers) {
        return nullptr;
    }

    const auto dataCopy = slot.data->copy();
    slot.deliveredCount++;

    if (slot.deliveredCount >= slot.expectedReceivers) {
        slot.data = nullptr;
        slot.rxCv.notify_all();
    }

    return dataCopy;
}

bool VirtualTransmitter::checkData(const std::string& name) {
    std::lock_guard<std::mutex> lock(s_mutex);
    const auto it = s_broadcastSlots.find(name);
    if (it == s_broadcastSlots.end()) {
        return false;
    }

    const auto& slot = it->second;
    return slot.data != nullptr && slot.deliveredCount < slot.expectedReceivers;
}

// ============================================================================
// Устаревшие методы
// ============================================================================

bool VirtualTransmitter::findTeg(const std::string& name) {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_broadcastSlots.find(name) != s_broadcastSlots.end();
}
