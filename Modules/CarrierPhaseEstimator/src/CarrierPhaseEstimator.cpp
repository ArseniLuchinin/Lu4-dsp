#include <CarrierPhaseEstimator.hpp>
#include <VariablesResolve.hpp>

#include <GpuFloatSignal.hpp>
#include <VirtualTransmitter.hpp>
#include <module.hpp>

#include <cuda_runtime.h>

#include <cmath>
#include <memory>

cudaError_t carrierPhaseQueryReduceStorageBytes(const cuComplex *in, size_t n,
                                                size_t *storageBytes);
cudaError_t carrierPhaseRunReduce(const cuComplex *in, size_t n,
                                  void *tempStorage, size_t tempStorageBytes,
                                  double2 *outSum);

IModule *createModule() { return new CarrierPhaseEstimator(); }

CarrierPhaseEstimator::CarrierPhaseEstimator()
    : IModule({"CarrierPhaseEstimator", "libCarrierPhaseEstimator-module.so",
               "module.json"}) {}

CarrierPhaseEstimator::~CarrierPhaseEstimator() {
  if (m_reduceTempStorage) {
    cudaFree(m_reduceTempStorage);
    m_reduceTempStorage = nullptr;
    m_reduceTempStorageBytes = 0;
  }
  if (m_reduceOut) {
    cudaFree(m_reduceOut);
    m_reduceOut = nullptr;
  }
}

bool CarrierPhaseEstimator::init() {
  if (m_phaseTag.empty()) {
    ERROR << "CarrierPhaseEstimator::init failed: phase tag is empty."
          << std::endl;
    return false;
  }
  return true;
}

bool CarrierPhaseEstimator::setData(std::shared_ptr<IData> data) {
  m_inData = std::dynamic_pointer_cast<GpuComplexFloatSignal>(data);
  if (!m_inData) {
    ERROR << "CarrierPhaseEstimator::setData failed: input must be "
             "GpuComplexFloatSignal."
          << std::endl;
    return false;
  }
  if (!m_inData->isValid()) {
    ERROR << "CarrierPhaseEstimator::setData failed: input signal is invalid."
          << std::endl;
    return false;
  }

  m_outData = m_inData;
  return true;
}

bool CarrierPhaseEstimator::run() {
  if (!m_inData) {
    ERROR << "CarrierPhaseEstimator::run failed: input data is null."
          << std::endl;
    return false;
  }

  const size_t n = m_inData->size();
  double2 sum = make_double2(0.0, 0.0);

  if (n > 0) {
    if (!m_reduceOut) {
      const auto allocOutErr =
          cudaMalloc(reinterpret_cast<void **>(&m_reduceOut), sizeof(double2));
      if (allocOutErr != cudaSuccess) {
        ERROR << "CarrierPhaseEstimator::run failed: unable to allocate reduce "
                 "output buffer: "
              << cudaGetErrorString(allocOutErr) << std::endl;
        return false;
      }
    }

    size_t requiredStorageBytes = 0;
    const auto queryErr = carrierPhaseQueryReduceStorageBytes(
        m_inData->getDeviceData(), n, &requiredStorageBytes);
    if (queryErr != cudaSuccess) {
      ERROR << "CarrierPhaseEstimator::run failed: unable to query CUB reduce "
               "storage: "
            << cudaGetErrorString(queryErr) << std::endl;
      return false;
    }

    if (requiredStorageBytes > m_reduceTempStorageBytes) {
      if (m_reduceTempStorage) {
        cudaFree(m_reduceTempStorage);
        m_reduceTempStorage = nullptr;
        m_reduceTempStorageBytes = 0;
      }

      const auto allocTempErr =
          cudaMalloc(reinterpret_cast<void **>(&m_reduceTempStorage),
                     requiredStorageBytes);
      if (allocTempErr != cudaSuccess) {
        ERROR << "CarrierPhaseEstimator::run failed: unable to allocate CUB "
                 "temp storage: "
              << cudaGetErrorString(allocTempErr) << std::endl;
        return false;
      }
      m_reduceTempStorageBytes = requiredStorageBytes;
    }

    const auto reduceErr = carrierPhaseRunReduce(
        m_inData->getDeviceData(), n, m_reduceTempStorage,
        m_reduceTempStorageBytes, static_cast<double2 *>(m_reduceOut));
    if (reduceErr != cudaSuccess) {
      ERROR << "CarrierPhaseEstimator::run failed: CUB reduce failed: "
            << cudaGetErrorString(reduceErr) << std::endl;
      return false;
    }

    const auto copySumErr =
        cudaMemcpy(&sum, m_reduceOut, sizeof(double2), cudaMemcpyDeviceToHost);
    if (copySumErr != cudaSuccess) {
      ERROR << "CarrierPhaseEstimator::run failed: unable to copy reduce "
               "result to host: "
            << cudaGetErrorString(copySumErr) << std::endl;
      return false;
    }
  }

  if (n == 0 || (sum.x == 0.0 && sum.y == 0.0)) {
    m_estimatedPhase = 0.0f;
  } else {
    constexpr float quarterPi = static_cast<float>(M_PI_4);
    constexpr float halfPi = static_cast<float>(M_PI_2);

    float phase =
        static_cast<float>(std::atan2(sum.y, sum.x) * 0.25) - quarterPi;
    while (phase >= quarterPi) {
      phase -= halfPi;
    }
    while (phase < -quarterPi) {
      phase += halfPi;
    }
    m_estimatedPhase = phase;
  }

  auto phaseData = std::make_shared<GpuFloatSignal>(1);
  if (!phaseData || !phaseData->isValid()) {
    ERROR << "CarrierPhaseEstimator::run failed: unable to allocate phase GPU "
             "buffer."
          << std::endl;
    return false;
  }
  phaseData->setDataFromHost(&m_estimatedPhase, 1);
  if (!phaseData->isValid()) {
    ERROR << "CarrierPhaseEstimator::run failed: unable to upload phase to GPU."
          << std::endl;
    return false;
  }

  VirtualTransmitter transmitter;
  transmitter.txData(phaseData, m_phaseTag);
  return true;
}

void CarrierPhaseEstimator::setParam(const std::string &paramName,
                                     const std::any &value) {
  if (paramName == "phase tag") {
    m_phaseTag = std::any_cast<std::string>(value);
    return;
  }

  ERROR << "CarrierPhaseEstimator::setParam unknown parameter: " << paramName
        << std::endl;
}

std::shared_ptr<IData> CarrierPhaseEstimator::getData() { return m_outData; }
