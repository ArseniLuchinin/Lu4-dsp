#include <CpuFloatSignal.hpp>
#include <cstring>
#include <iostream>

// Конструктор
CpuFloatSignal::CpuFloatSignal(float *data, size_t size)
    : m_data(data), m_size(size), IData("CPU float signal") {}

// Статический фабричный метод fromGpu
std::shared_ptr<CpuFloatSignal>
CpuFloatSignal::fromGpu(std::shared_ptr<IData> iData) {
  auto gpuData = std::dynamic_pointer_cast<GpuFloatSignal>(iData);
  if (gpuData == nullptr) {
    std::cerr << "CpuFloatSignal::fromGpu: Input IData is not a GpuFloatSignal."
              << std::endl;
    return std::make_shared<CpuFloatSignal>();
  }

  if (gpuData->size() == 0) {
    std::cerr << "CpuFloatSignal::fromGpu: GpuFloatSignal has size 0."
              << std::endl;
    return std::make_shared<CpuFloatSignal>();
  }

  float *data = new float[gpuData->size()];
  cudaError_t err =
      cudaMemcpy(data, gpuData->getDeviceData(),
                 gpuData->size() * sizeof(float), cudaMemcpyDeviceToHost);
  if (err != cudaSuccess) {
    std::cerr << "CpuFloatSignal::fromGpu: cudaMemcpy failed: "
              << cudaGetErrorString(err) << std::endl;
    delete[] data;
    return std::make_shared<CpuFloatSignal>();
  }
  return std::make_shared<CpuFloatSignal>(data, gpuData->size());
}

// Деструктор
CpuFloatSignal::~CpuFloatSignal() {
  if (m_data) {
    delete[] m_data;
    m_size = 0;
    m_data = nullptr;
  }
}

size_t CpuFloatSignal::size() const { return m_size; }

float *CpuFloatSignal::getData() const { return m_data; }

std::shared_ptr<IData> CpuFloatSignal::copy() const {
  if (!isValid()) {
    std::cerr << "CpuFloatSignal::copy failed: source data is invalid."
              << std::endl;
    return nullptr;
  }

  float *newData = new float[m_size];
  std::memcpy(newData, m_data, m_size * sizeof(float));

  return std::make_shared<CpuFloatSignal>(newData, m_size);
}
