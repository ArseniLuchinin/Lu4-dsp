#ifndef GPU_FLOAT_SIGNAL_HPP
#define GPU_FLOAT_SIGNAL_HPP

#include <IData.hpp>

/**
 * @brief Контейнер для сигнала `float`, размещенного в памяти GPU.
 *
 * Класс владеет буфером устройства (`m_data`) и освобождает его в деструкторе.
 * Размер сигнала хранится в `m_size` и интерпретируется как количество элементов `float`.
 */
class GpuFloatSignal : public IData {
public:
    /// @brief Создает пустой объект без выделенного GPU-буфера.
    GpuFloatSignal() = default;

    /**
     * @brief Создает сигнал заданного размера и выделяет память на устройстве.
     * @param size Количество элементов `float` в сигнале.
     */
    GpuFloatSignal(size_t size);

    /**
     * @brief Перемещающий конструктор.
     * @param other Объект-источник перемещения.
     */
    GpuFloatSignal(GpuFloatSignal && other);
    
    /**
     * @brief Копирует данные из оперативной памяти в GPU-буфер.
     *
     * Метод обновляет `m_size` и выполняет копирование `cudaMemcpyHostToDevice`.
     * Предполагается, что буфер устройства уже выделен и имеет достаточный размер.
     *
     * @param data Указатель на данные в памяти хоста.
     * @param size Количество элементов `float` для копирования.
     */
    void setDataFromHost(float* data, size_t size);

    /**
     * @brief Копирует данные между буферами устройства.
     * @param data Указатель на исходные данные в памяти GPU.
     * @param size Количество элементов `float` для копирования.
     */
    void setDataFromDevice(float* data, const size_t size);

    /**
     * @brief Возвращает имя типа данных.
     * @return Строка `"Gpu float signal"`.
     */
    std::string getDataName() const override {
        return "Gpu float signal";
    }

    size_t size() const override {
        return m_size;
    }

    /// @brief Освобождает внутренний GPU-буфер.
    ~GpuFloatSignal();

    /**
     * @brief Возвращает указатель на данные в памяти GPU.
     * @return Указатель на внутренний буфер устройства.
     */
    float* getDeviceData();


protected:
    void freeData();

    size_t m_size = 0;
    float* m_data = nullptr;
};

#endif
