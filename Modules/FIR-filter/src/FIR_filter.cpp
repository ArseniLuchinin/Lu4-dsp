#include <FIR_filter.hpp>
#include <VariablesResolve.hpp>

#include <GpuSignalUtils.hpp>

#include <cstdint>
#include <cstddef>
#include <string>

#include <cuda_runtime.h>
#include <module.hpp>

IModule* createModule() {
    return new FIRFilter();
}

FIRFilter::FIRFilter() : IModule({"FIR-filter", "", ""}) {}
FIRFilter::~FIRFilter()
{
    if (m_energyBuffer) {
        cudaFree(m_energyBuffer);
        m_energyBuffer = nullptr;
    }
}

bool FIRFilter::init()
{
    if (m_M <= 0 || (m_M % 2) == 0) {
        ERROR << "FIRFilter::init failed: filter order must be positive odd number." << std::endl;
        return false;
    }

    std::shared_ptr<IData> rxBase = rxData();
    if (!rxBase) {
        ERROR << "FIRFilter::init failed: coefficients source is nullptr." << std::endl;
        return false;
    }

    auto realTaps = std::dynamic_pointer_cast<GpuFloatSignal>(rxBase);
    auto complexTaps = std::dynamic_pointer_cast<GpuComplexFloatSignal>(rxBase);

    if (!realTaps && !complexTaps) {
        ERROR << "FIRFilter::init failed: coefficients source must be GpuFloatSignal or GpuComplexFloatSignal." << std::endl;
        return false;
    }

    if (realTaps && !realTaps->isValid()) {
        ERROR << "FIRFilter::init failed: real coefficients data is invalid." << std::endl;
        return false;
    }
    if (complexTaps && !complexTaps->isValid()) {
        ERROR << "FIRFilter::init failed: complex coefficients data is invalid." << std::endl;
        return false;
    }

    const size_t tapsSize = realTaps ? realTaps->size() : complexTaps->size();
    if (tapsSize != static_cast<size_t>(m_M)) {
        ERROR << "FIRFilter::init failed: coefficients size must match filter order." << std::endl;
        return false;
    }

    if (realTaps) {
        if (m_coefficientsTypeMode == CoefficientsTypeMode::Complex) {
            ERROR << "FIRFilter::init failed: coefficients type mode is 'complex', but real taps were provided." << std::endl;
            return false;
        }

        m_realTaps = realTaps;

        m_complexTaps.reset();
        m_tapType = TapType::Real;
    } else {
        if (m_coefficientsTypeMode == CoefficientsTypeMode::Real) {
            ERROR << "FIRFilter::init failed: coefficients type mode is 'real', but complex taps were provided." << std::endl;
            return false;
        }

        m_complexTaps = complexTaps;

        m_realTaps.reset();
        m_tapType = TapType::Complex;
    }

    m_data.reset();
    m_gpuData.reset();
    m_workData.reset();
    m_historyData.reset();
    m_nextHistoryData.reset();

    INFO << "FIRFilter::init: taps initialized, order=" << m_M
         << ", tap type=" << (m_tapType == TapType::Real ? "real" : "complex") << std::endl;
    return true;
}

bool FIRFilter::setData(std::shared_ptr<IData> data){
    if (m_M <= 0) {
        ERROR << "FIRFilter::setData failed: filter order is not initialized." << std::endl;
        return false;
    }

    if (m_tapType == TapType::None) {
        ERROR << "FIRFilter::setData failed: taps are not initialized." << std::endl;
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

    if (m_tapType == TapType::Complex && m_gpuData->sampleType() != GpuSampleType::ComplexFloat32) {
        ERROR << "FIRFilter::setData failed: complex taps require complex input signal." << std::endl;
        return false;
    }

    const bool needWorkRecreate =
        !m_workData ||
        m_workData->sampleType() != m_gpuData->sampleType() ||
        m_workData->availableSize() < m_gpuData->size();

    if (needWorkRecreate) {
        m_workData = createLike(*m_gpuData, m_gpuData->size());
        if (!m_workData || !m_workData->isValid()) {
            ERROR << "FIRFilter::setData failed: unable to allocate work buffer." << std::endl;
            return false;
        }
    } else if (!m_workData->setLogicalSize(m_gpuData->size())) {
        ERROR << "FIRFilter::setData failed: unable to set work buffer size." << std::endl;
        return false;
    }

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

    if (paramName == "coefficients type") {
        const auto modeStr = std::any_cast<std::string>(resolved);
        if (modeStr == "auto") {
            m_coefficientsTypeMode = CoefficientsTypeMode::Auto;
            return;
        }
        if (modeStr == "real") {
            m_coefficientsTypeMode = CoefficientsTypeMode::Real;
            return;
        }
        if (modeStr == "complex") {
            m_coefficientsTypeMode = CoefficientsTypeMode::Complex;
            return;
        }
        {
            ERROR << "FIRFilter::setParam failed: unknown coefficients type '" << modeStr << "'." << std::endl;
        }
        return;
    }

    if (paramName == "log energy") {
        m_logEnergy = std::any_cast<bool>(resolved);
        return;
    }

    if (paramName == "block size") {
        (void)std::any_cast<int32_t>(resolved);
        DEBUG << "FIRFilter::setParam: 'block size' is accepted for compatibility and ignored." << std::endl;
        return;
    }

    ERROR << "can't handle param: " << paramName << std::endl;
}

std::shared_ptr<IData> FIRFilter::getData()
{
    return m_data;
}
