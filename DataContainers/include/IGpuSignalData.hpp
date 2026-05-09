#ifndef I_GPU_SIGNAL_DATA_HPP
#define I_GPU_SIGNAL_DATA_HPP

#include <IData.hpp>

#include <cstddef>

enum class GpuSampleType { Float32, ComplexFloat32, UInt8 };

/*!
 * @brief Базовый runtime-интерфейс для GPU сигналов.
 * @details
 * Позволяет работать с контейнерами GPU-данных в обобщённом виде
 * без знания конкретного типа элемента (float/complex float)
 * до момента непосредственного запуска вычислений.
 */
class IGpuSignalData : public IData {
public:
  IGpuSignalData() = delete;
  explicit IGpuSignalData(const std::string &dataName) : IData(dataName) {}

  ~IGpuSignalData() override = default;

  /// @brief Тип семпла внутри GPU-контейнера.
  virtual GpuSampleType sampleType() const = 0;
  /// @brief Неструктурированный указатель на данные в памяти GPU.
  virtual void *deviceDataRaw() = 0;
  /// @brief Константная версия неструктурированного указателя на данные в
  /// памяти GPU.
  virtual const void *deviceDataRaw() const = 0;
  /// @brief Размер одного элемента контейнера в байтах.
  virtual size_t elementSizeBytes() const = 0;
  /// @brief Устанавливает логический размер контейнера в элементах.
  virtual bool setLogicalSize(size_t size) = 0;
};

#endif // I_GPU_SIGNAL_DATA_HPP
