#ifndef GPU_SIGNAL_UTILS_HPP
#define GPU_SIGNAL_UTILS_HPP

#include <IData.hpp>
#include <IGpuSignalData.hpp>
#include <GpuFloatSignal.hpp>
#include <GpuComplexSignal.hpp>

#include <initializer_list>
#include <memory>
#include <string>
#include <cuda_runtime.h>

/*!
 * @brief Результат валидации/приведения GPU-сигнала.
 * @details
 * Если ok == false, поле error содержит текст ошибки.
 * В успешном случае signal содержит валидный IGpuSignalData.
 */
struct ValidationResult {
    bool ok = false;
    std::string error;
    std::shared_ptr<IGpuSignalData> signal;
};

/*!
 * @brief Безопасно приводит IData к IGpuSignalData.
 * @param data Входной контейнер.
 * @return ValidationResult с заполненным signal при успехе.
 */
inline ValidationResult asGpuSignal(const std::shared_ptr<IData>& data)
{
    if (!data) {
        return {false, "Input data is nullptr.", nullptr};
    }

    auto gpu = std::dynamic_pointer_cast<IGpuSignalData>(data);
    if (!gpu) {
        return {false, "Input IData is not IGpuSignalData.", nullptr};
    }

    return {true, "", gpu};
}

/*!
 * @brief Проверяет, входит ли тип сигнала в разрешённый набор.
 * @param value Проверяемый тип.
 * @param allowed Список допустимых типов.
 */
inline bool isAllowedType(
    GpuSampleType value,
    std::initializer_list<GpuSampleType> allowed)
{
    for (const auto type : allowed) {
        if (type == value) {
            return true;
        }
    }
    return false;
}

/*!
 * @brief Унифицированная проверка входного GPU-контейнера.
 * @param data Входной IData.
 * @param allowedTypes Разрешённые типы семплов.
 * @param minSize Минимально допустимый размер (в элементах).
 * @param context Префикс для текста ошибки (например, имя метода).
 * @return ValidationResult с текстом ошибки или валидным сигналом.
 */
inline ValidationResult validateGpuInput(
    const std::shared_ptr<IData>& data,
    std::initializer_list<GpuSampleType> allowedTypes,
    size_t minSize,
    const std::string& context)
{
    auto castResult = asGpuSignal(data);
    if (!castResult.ok) {
        castResult.error = context + ": " + castResult.error;
        return castResult;
    }

    const auto& signal = castResult.signal;
    if (!signal->isValid()) {
        return {false, context + ": GPU input is invalid.", nullptr};
    }

    if (!isAllowedType(signal->sampleType(), allowedTypes)) {
        return {false, context + ": unsupported GPU sample type.", nullptr};
    }

    if (signal->size() < minSize) {
        return {false, context + ": input size is smaller than required minimum.", nullptr};
    }

    if (!signal->deviceDataRaw()) {
        return {false, context + ": device pointer is nullptr.", nullptr};
    }

    return {true, "", signal};
}

/*!
 * @brief Создаёт GPU-контейнер такого же типа, как у prototype.
 * @param prototype Образец типа (float/complex float).
 * @param size Размер нового контейнера.
 * @return Новый IGpuSignalData или nullptr, если тип не поддержан.
 */
inline std::shared_ptr<IGpuSignalData> createLike(const IGpuSignalData& prototype, size_t size)
{
    switch (prototype.sampleType()) {
    case GpuSampleType::Float32:
        return std::make_shared<GpuFloatSignal>(size);
    case GpuSampleType::ComplexFloat32:
        return std::make_shared<GpuComplexFloatSignal>(size);
    default:
        return nullptr;
    }
}

/*!
 * @brief Гарантирует наличие буферов истории того же типа, что и вход.
 * @details
 * При необходимости пересоздаёт буферы, либо переиспользует существующие.
 * Используется для сценариев скользящей обработки (например, FIR history).
 *
 * @param input Входной сигнал, по которому определяется тип буферов.
 * @param historySize Требуемый размер буферов истории.
 * @param history Буфер прошлой истории (in/out).
 * @param nextHistory Буфер следующей истории (in/out).
 * @return ValidationResult с ok=true при успехе.
 */
inline ValidationResult ensureHistoryLike(
    const IGpuSignalData& input,
    size_t historySize,
    std::shared_ptr<IGpuSignalData>& history,
    std::shared_ptr<IGpuSignalData>& nextHistory)
{
    if (historySize == 0) {
        history.reset();
        nextHistory.reset();
        return {true, "", nullptr};
    }

    const auto needsRecreate = [&](const std::shared_ptr<IGpuSignalData>& buffer) {
        return !buffer ||
               buffer->sampleType() != input.sampleType() ||
               buffer->availableSize() < historySize;
    };

    if (needsRecreate(history)) {
        history = createLike(input, historySize);
        if (!history || !history->isValid()) {
            return {false, "ensureHistoryLike: failed to create history buffer.", nullptr};
        }

        const auto clearErr = cudaMemset(
            history->deviceDataRaw(),
            0,
            historySize * history->elementSizeBytes());
        if (clearErr != cudaSuccess) {
            return {false, "ensureHistoryLike: failed to zero-initialize history buffer.", nullptr};
        }
    } else if (!history->setLogicalSize(historySize)) {
        return {false, "ensureHistoryLike: failed to set history logical size.", nullptr};
    }

    if (needsRecreate(nextHistory)) {
        nextHistory = createLike(input, historySize);
        if (!nextHistory || !nextHistory->isValid()) {
            return {false, "ensureHistoryLike: failed to create next history buffer.", nullptr};
        }
    } else if (!nextHistory->setLogicalSize(historySize)) {
        return {false, "ensureHistoryLike: failed to set next history logical size.", nullptr};
    }

    return {true, "", nullptr};
}

#endif // GPU_SIGNAL_UTILS_HPP
