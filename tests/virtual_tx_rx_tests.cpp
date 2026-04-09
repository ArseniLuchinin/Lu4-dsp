#include <VirtualTransmitter.hpp>
#include <GpuFloatSignal.hpp>
#include <EmptyContainer.hpp>
#include <cuda_runtime.h>

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

namespace {

bool isCudaAvailable()
{
    int deviceCount = 0;
    return cudaGetDeviceCount(&deviceCount) == cudaSuccess && deviceCount > 0;
}

std::shared_ptr<GpuFloatSignal> makeGpuFloatSignal(size_t size, float value)
{
    auto signal = std::make_shared<GpuFloatSignal>(size);
    if (!signal->isValid()) {
        return nullptr;
    }

    std::vector<float> hostData(size, value);
    signal->setDataFromHost(hostData.data(), size);
    return signal;
}

std::vector<float> downloadGpuFloat(const std::shared_ptr<GpuFloatSignal>& data)
{
    if (!data || !data->isValid() || !data->getDeviceData()) {
        return {};
    }

    std::vector<float> out(data->size());
    const auto err = cudaMemcpy(out.data(), data->getDeviceData(),
                                out.size() * sizeof(float), cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        return {};
    }
    return out;
}

// Сброс статического состояния между тестами
void resetTransmitterState()
{
    VirtualTransmitter::setTimeoutMs(5000);  // 5 сек для тестов
}

} // namespace

// ============================================================================
// Обратная совместимость: 1 TX → 1 RX
// ============================================================================

TEST(BroadcastTest, OneTxToOneRx_DataTransferred)
{
    if (!isCudaAvailable()) {
        GTEST_SKIP() << "CUDA device is unavailable.";
    }

    resetTransmitterState();

    VirtualTransmitter tx;
    tx.registerTx("tag_1to1");
    tx.registerRx("tag_1to1");

    auto original = makeGpuFloatSignal(10, 42.0f);
    ASSERT_NE(original, nullptr);

    tx.txData(original, "tag_1to1");

    auto received = tx.waitRxData("tag_1to1");
    ASSERT_NE(received, nullptr);

    auto receivedGpu = std::dynamic_pointer_cast<GpuFloatSignal>(received);
    ASSERT_NE(receivedGpu, nullptr);
    EXPECT_EQ(receivedGpu->size(), 10u);

    auto hostData = downloadGpuFloat(receivedGpu);
    ASSERT_EQ(hostData.size(), 10u);
    for (const auto v : hostData) {
        EXPECT_FLOAT_EQ(v, 42.0f);
    }
}

// ============================================================================
// Broadcaster: 1 TX → 2 RX
// ============================================================================

TEST(BroadcastTest, OneTxToTwoRx_BothReceiveSameData)
{
    if (!isCudaAvailable()) {
        GTEST_SKIP() << "CUDA device is unavailable.";
    }

    resetTransmitterState();

    VirtualTransmitter tx;
    tx.registerTx("tag_1to2");
    tx.registerRx("tag_1to2");
    tx.registerRx("tag_1to2");

    auto original = makeGpuFloatSignal(5, 7.5f);
    ASSERT_NE(original, nullptr);

    tx.txData(original, "tag_1to2");

    // Оба RX получают данные (параллельно)
    auto rx1 = tx.waitRxData("tag_1to2");
    auto rx2 = tx.waitRxData("tag_1to2");

    ASSERT_NE(rx1, nullptr);
    ASSERT_NE(rx2, nullptr);

    auto rx1Gpu = std::dynamic_pointer_cast<GpuFloatSignal>(rx1);
    auto rx2Gpu = std::dynamic_pointer_cast<GpuFloatSignal>(rx2);
    ASSERT_NE(rx1Gpu, nullptr);
    ASSERT_NE(rx2Gpu, nullptr);

    // Оба получили одинаковые данные
    auto host1 = downloadGpuFloat(rx1Gpu);
    auto host2 = downloadGpuFloat(rx2Gpu);
    ASSERT_EQ(host1.size(), 5u);
    ASSERT_EQ(host2.size(), 5u);
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_FLOAT_EQ(host1[i], 7.5f);
        EXPECT_FLOAT_EQ(host2[i], 7.5f);
    }

    // Копии независимы — это разные объекты
    EXPECT_NE(rx1Gpu->getDeviceData(), rx2Gpu->getDeviceData());
}

// ============================================================================
// Broadcaster: 1 TX → 3 RX
// ============================================================================

TEST(BroadcastTest, OneTxToThreeRx_AllReceiveData)
{
    if (!isCudaAvailable()) {
        GTEST_SKIP() << "CUDA device is unavailable.";
    }

    resetTransmitterState();

    VirtualTransmitter tx;
    tx.registerTx("tag_1to3");
    tx.registerRx("tag_1to3");
    tx.registerRx("tag_1to3");
    tx.registerRx("tag_1to3");

    auto original = makeGpuFloatSignal(3, 99.0f);
    ASSERT_NE(original, nullptr);

    tx.txData(original, "tag_1to3");

    auto rx1 = tx.waitRxData("tag_1to3");
    auto rx2 = tx.waitRxData("tag_1to3");
    auto rx3 = tx.waitRxData("tag_1to3");

    ASSERT_NE(rx1, nullptr);
    ASSERT_NE(rx2, nullptr);
    ASSERT_NE(rx3, nullptr);

    EXPECT_EQ(rx1->size(), 3u);
    EXPECT_EQ(rx2->size(), 3u);
    EXPECT_EQ(rx3->size(), 3u);
}

// ============================================================================
// Последний RX получает оригинал (без копирования)
// ============================================================================

TEST(BroadcastTest, LastRxGetsOriginal_NotCopy)
{
    if (!isCudaAvailable()) {
        GTEST_SKIP() << "CUDA device is unavailable.";
    }

    resetTransmitterState();

    VirtualTransmitter tx;
    tx.registerTx("tag_move");
    tx.registerRx("tag_move");
    tx.registerRx("tag_move");

    auto original = makeGpuFloatSignal(5, 42.0f);
    ASSERT_NE(original, nullptr);
    const void* originalPtr = original->getDeviceData();

    tx.txData(original, "tag_move");

    // Первый RX — должен получить копию (другой указатель)
    auto rx1 = tx.waitRxData("tag_move");
    ASSERT_NE(rx1, nullptr);
    auto rx1Gpu = std::dynamic_pointer_cast<GpuFloatSignal>(rx1);
    ASSERT_NE(rx1Gpu, nullptr);
    EXPECT_NE(rx1Gpu->getDeviceData(), originalPtr);  // копия ≠ оригинал

    // Второй (последний) RX — должен получить оригинал (тот же указатель)
    auto rx2 = tx.waitRxData("tag_move");
    ASSERT_NE(rx2, nullptr);
    auto rx2Gpu = std::dynamic_pointer_cast<GpuFloatSignal>(rx2);
    ASSERT_NE(rx2Gpu, nullptr);
    EXPECT_EQ(rx2Gpu->getDeviceData(), originalPtr);  // оригинал!
}

// ============================================================================
// Нормальный сценарий: TX не блокируется когда RX забирает данные
// ============================================================================

TEST(BroadcastTest, TxDoesNotBlock_WhenRxConsumesPromptly)
{
    if (!isCudaAvailable()) {
        GTEST_SKIP() << "CUDA device is unavailable.";
    }

    resetTransmitterState();

    VirtualTransmitter tx;
    tx.registerTx("tag_block");
    tx.registerRx("tag_block");

    // Первая отправка — проходит сразу
    auto data1 = makeGpuFloatSignal(4, 1.0f);
    ASSERT_NE(data1, nullptr);
    tx.txData(data1, "tag_block");

    // RX забирает данные
    auto rx1 = tx.waitRxData("tag_block");
    ASSERT_NE(rx1, nullptr);

    // Вторая отправка — слот свободен, проходит сразу
    auto data2 = makeGpuFloatSignal(4, 2.0f);
    ASSERT_NE(data2, nullptr);
    tx.txData(data2, "tag_block");

    // RX забирает вторую порцию
    auto rx2 = tx.waitRxData("tag_block");
    ASSERT_NE(rx2, nullptr);

    auto rx2Gpu = std::dynamic_pointer_cast<GpuFloatSignal>(rx2);
    auto hostData = downloadGpuFloat(rx2Gpu);
    ASSERT_EQ(hostData.size(), 4u);
    for (const auto v : hostData) {
        EXPECT_FLOAT_EQ(v, 2.0f);
    }
}

// ============================================================================
// TX блокируется если RX не забрал предыдущие данные (реальная блокировка)
// ============================================================================

TEST(BroadcastTest, TxBlocks_WhenRxDoesNotConsume)
{
    if (!isCudaAvailable()) {
        GTEST_SKIP() << "CUDA device is unavailable.";
    }

    resetTransmitterState();
    VirtualTransmitter::setTimeoutMs(3000);  // 3 сек для теста

    VirtualTransmitter tx;
    tx.registerTx("tag_real_block");
    tx.registerRx("tag_real_block");

    // TX отправляет первую порцию
    auto data1 = makeGpuFloatSignal(4, 10.0f);
    ASSERT_NE(data1, nullptr);
    tx.txData(data1, "tag_real_block");

    // Флаг: TX разблокировался
    std::atomic<bool> txUnblocked{false};

    // Запускаем TX в отдельном потоке — он должен заблокироваться
    auto txThread = std::thread([&]() {
        auto data2 = makeGpuFloatSignal(4, 20.0f);
        if (data2) {
            tx.txData(data2, "tag_real_block");  // Здесь TX заблокируется
        }
        txUnblocked.store(true);
    });

    // Даём TX время чтобы он попытался отправить и заблокировался
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // TX ещё не разблокировался (ждёт RX)
    EXPECT_FALSE(txUnblocked.load());

    // RX забирает первую порцию — это разблокирует TX
    auto rx1 = tx.waitRxData("tag_real_block");
    ASSERT_NE(rx1, nullptr);

    // Ждём разблокировки TX
    txThread.join();

    // TX разблокировался после того как RX забрал данные
    EXPECT_TRUE(txUnblocked.load());

    // RX получает вторую порцию
    auto rx2 = tx.waitRxData("tag_real_block");
    ASSERT_NE(rx2, nullptr);

    auto rx2Gpu = std::dynamic_pointer_cast<GpuFloatSignal>(rx2);
    auto hostData = downloadGpuFloat(rx2Gpu);
    ASSERT_EQ(hostData.size(), 4u);
    for (const auto v : hostData) {
        EXPECT_FLOAT_EQ(v, 20.0f);
    }
}

// ============================================================================
// Валидация: дубликат TX на том же теге
// ============================================================================

TEST(BroadcastTest, DuplicateTxOnSameTag_Fails)
{
    resetTransmitterState();

    VirtualTransmitter tx1;
    VirtualTransmitter tx2;

    bool result1 = tx1.registerTx("tag_conflict");
    EXPECT_TRUE(result1);

    bool result2 = tx2.registerTx("tag_conflict");
    EXPECT_FALSE(result2);
}

TEST(BroadcastTest, SameTagRegisteredOnce_Fails)
{
    resetTransmitterState();

    VirtualTransmitter tx1;
    VirtualTransmitter tx2;

    bool result1 = tx1.registerTx("tag_same");
    EXPECT_TRUE(result1);

    bool result2 = tx2.registerTx("tag_same");
    EXPECT_FALSE(result2);
}

// ============================================================================
// Авто-инкремент RX через registerRx
// ============================================================================

TEST(BroadcastTest, RxAutoIncrement_CorrectCount)
{
    resetTransmitterState();

    VirtualTransmitter tx;
    tx.registerTx("tag_count");

    // Регистрируем 3 RX
    tx.registerRx("tag_count");
    tx.registerRx("tag_count");
    tx.registerRx("tag_count");

    // Отправляем данные
    auto data = makeGpuFloatSignal(2, 5.0f);
    if (data) {
        tx.txData(data, "tag_count");
    }

    // Все 3 RX должны получить данные
    auto rx1 = tx.waitRxData("tag_count");
    auto rx2 = tx.waitRxData("tag_count");
    auto rx3 = tx.waitRxData("tag_count");

    EXPECT_NE(rx1, nullptr);
    EXPECT_NE(rx2, nullptr);
    EXPECT_NE(rx3, nullptr);

    // После того как все получили — слот должен быть свободен
    // Следующая отправка должна пройти без блокировки
    auto data2 = makeGpuFloatSignal(2, 10.0f);
    if (data2) {
        tx.txData(data2, "tag_count");
    }

    auto rx4 = tx.waitRxData("tag_count");
    EXPECT_NE(rx4, nullptr);
}

// ============================================================================
// checkData — проверка наличия данных
// ============================================================================

TEST(BroadcastTest, CheckData_ReturnsTrueWhenDataAvailable)
{
    resetTransmitterState();

    VirtualTransmitter tx;
    tx.registerTx("tag_check");
    tx.registerRx("tag_check");

    // До отправки — данных нет
    EXPECT_FALSE(tx.checkData("tag_check"));

    // Отправляем данные
    auto data = makeGpuFloatSignal(1, 3.14f);
    if (data) {
        tx.txData(data, "tag_check");
    }

    // После отправки — данные есть
    EXPECT_TRUE(tx.checkData("tag_check"));

    // RX забирает данные
    auto rx = tx.waitRxData("tag_check");
    ASSERT_NE(rx, nullptr);

    // После получения — данных снова нет
    EXPECT_FALSE(tx.checkData("tag_check"));
}

// ============================================================================
// rxData — неблокирующее получение
// ============================================================================

TEST(BroadcastTest, RxData_NonBlocking_ReturnsNullWhenEmpty)
{
    resetTransmitterState();

    VirtualTransmitter tx;
    tx.registerTx("tag_nonblock");
    tx.registerRx("tag_nonblock");

    // До отправки — nullptr
    EXPECT_EQ(tx.rxData("tag_nonblock"), nullptr);

    // Отправляем данные
    auto data = makeGpuFloatSignal(2, 1.5f);
    if (data) {
        tx.txData(data, "tag_nonblock");
    }

    // Первое получение — данные есть
    auto rx1 = tx.rxData("tag_nonblock");
    EXPECT_NE(rx1, nullptr);

    // Второй RX — данные всё ещё есть (первый ещё не забрал все)
    // Но в случае 1 RX — после первого rxData слот очищается
    auto rx2 = tx.rxData("tag_nonblock");
    EXPECT_EQ(rx2, nullptr);  // слот уже пуст
}
