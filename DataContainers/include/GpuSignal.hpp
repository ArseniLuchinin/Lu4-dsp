#ifndef GPU_SIGNAL_HPP
#define GPU_SIGNAL_HPP

#include <IGpuSignalData.hpp>

#include <cstddef>
#include <cstdint>
#include <cuComplex.h>

template <typename T> struct GpuSampleTypeTraits;

template <> struct GpuSampleTypeTraits<float> {
  static constexpr GpuSampleType value = GpuSampleType::Float32;
};

template <> struct GpuSampleTypeTraits<cuComplex> {
  static constexpr GpuSampleType value = GpuSampleType::ComplexFloat32;
};

template <> struct GpuSampleTypeTraits<uint8_t> {
  static constexpr GpuSampleType value = GpuSampleType::UInt8;
};

template <typename T, typename Tag> class GpuSignal : public IGpuSignalData {
public:
  GpuSignal(const GpuSignal &) = delete;
  GpuSignal &operator=(const GpuSignal &) = delete;

  GpuSignal();
  explicit GpuSignal(size_t size);
  GpuSignal(GpuSignal &&other) noexcept;
  GpuSignal &operator=(GpuSignal &&other) noexcept;
  ~GpuSignal() override;

  bool reserve(const size_t size) override;
  void setDataFromHost(T *data, size_t size);
  void setDataFromDevice(T *data, size_t size);
  bool setLogicalSize(size_t size) override;

  size_t size() const override;
  size_t availableSize() const override;
  bool isValid() const override;

  GpuSampleType sampleType() const override;
  void *deviceDataRaw() override;
  const void *deviceDataRaw() const override;
  size_t elementSizeBytes() const override;

  T *getDeviceData();
  const T *getDeviceData() const;

  const char *name() const;

  std::shared_ptr<IData> copy() const override;

protected:
  void freeData();
  bool checkData(const T *data);

  size_t m_size = 0;
  size_t m_resedSize = 0;
  bool m_siValid = false;

  T *m_data = nullptr;
};

#endif
