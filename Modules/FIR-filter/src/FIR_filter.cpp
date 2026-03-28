#include <FIR_filter.hpp>
#include <VariablesResolve.hpp>
#include <CpuFloatSignal.hpp>
#include <GpuSignalUtils.hpp>

#include <cstdint>
#include <cstddef>
#include <cuda_runtime.h>
#include <cuComplex.h>
#include <module.hpp>

// Внешние объявления, что бы не выносить в .hpp файлы
extern __constant__ float d_h[];

namespace {
constexpr int kMaxFIRLength = 2048;
}

IModule* createModule() {
    return new FIRFilter();
}


FIRFilter::FIRFilter() : IModule({"FIR-filter", "", ""}) {}


bool FIRFilter::init()
{
    if (m_M <= 0 || (m_M % 2) == 0) {
        ERROR << "FIRFilter::init failed: filter order must be positive odd number." << std::endl;
        return false;
    }

    if (m_M > kMaxFIRLength) {
        ERROR << "FIRFilter::init failed: filter order exceeds max constant-memory taps." << std::endl;
        return false;
    }

    std::shared_ptr<CpuFloatSignal> rx = std::dynamic_pointer_cast<CpuFloatSignal>(rxData());
    if (!rx) {
        ERROR << "FIRFilter::init failed: coefficients source is not CpuFloatSignal." << std::endl;
        return false;
    }
    if (!rx->isValid()) {
        ERROR << "FIRFilter::init failed: coefficients data is invalid." << std::endl;
        return false;
    }
    if (rx->size() != static_cast<size_t>(m_M)) {
        ERROR << "FIRFilter::init failed: coefficients size must match filter order." << std::endl;
        return false;
    }

    INFO << "rx: " << rx->getDataName() << " size: " << rx->size() << std::endl;

    // Копируем в constant memory
    const auto err = cudaMemcpyToSymbol(d_h, rx->getData(), m_M * sizeof(float));
    if(err != cudaSuccess){
        ERROR << "coefs load failed: " << cudaGetErrorString(err) << std::endl;
        return false;
    }
    INFO << "coefs init" << std::endl;

    const size_t historySize = static_cast<size_t>(m_M - 1);
    (void)historySize;

    m_data.reset();
    m_gpuData.reset();
    m_historyData.reset();
    m_nextHistoryData.reset();

    return true;
}

bool FIRFilter::setData(std::shared_ptr<IData> data){
    if (m_M <= 0) {
        ERROR << "FIRFilter::setData failed: filter order is not initialized." << std::endl;
        return false;
    }

    const size_t historySize = static_cast<size_t>(m_M - 1);
    auto validation = validateGpuInput(
        data,
        {GpuSampleType::Float32, GpuSampleType::ComplexFloat32},
        historySize,
        "FIRFilter::setData");
    if (!validation.ok) {
        ERROR << validation.error << std::endl;
        return false;
    }

    m_gpuData = validation.signal;
    m_data = data;

    auto historyValidation = ensureHistoryLike(
        *m_gpuData, historySize, m_historyData, m_nextHistoryData);
    if (!historyValidation.ok) {
        ERROR << "FIRFilter::setData failed: " << historyValidation.error << std::endl;
        return false;
    }

    if (historySize == 0) {
        return true;
    }

    const auto elementSize = m_gpuData->elementSizeBytes();
    const auto offsetBytes = (m_gpuData->size() - historySize) * elementSize;
    const auto totalBytes = historySize * elementSize;

    const auto* inputBytes =
        static_cast<const std::byte*>(m_gpuData->deviceDataRaw());
    auto* nextHistoryBytes =
        static_cast<std::byte*>(m_nextHistoryData->deviceDataRaw());
    const auto err = cudaMemcpy(
        nextHistoryBytes,
        inputBytes + offsetBytes,
        totalBytes,
        cudaMemcpyDeviceToDevice
    );
    if (err != cudaSuccess) {
        ERROR << "FIRFilter::setData failed: cudaMemcpy next history failed: "
              << cudaGetErrorString(err) << std::endl;
        return false;
    }

    return true;
}

void FIRFilter::setParam(const std::string& paramName, const std::any& value) {
    const std::any resolved = resolveParamValue(value);
    if(paramName == "coefficients data tag"){
        setTag(std::any_cast<std::string>(resolved));
        return;
    }

    if (paramName == "filter order") {
        m_M = std::any_cast<int32_t>(resolved);
        return;
    }

    ERROR << "can't handle param: " << paramName << std::endl;
}


std::shared_ptr<IData> FIRFilter::getData()
{
    return m_data;
}
