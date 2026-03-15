#include "FFT_cufft.hpp"
#include "fftUtils.hpp"

#include <module.hpp>
#include <utility>
#include <iostream>

#include <cufft.h>
#include <cuda_runtime.h>


IModule* createModule() {
    return new FFT();
}

FFT::FFT() : IModule({"FFT", "FFT-module.so", "FFT.json"}) {}
FFT::~FFT() {
    if (m_plan != 0) {
        cufftDestroy(m_plan);
        m_plan = 0;
    }

    if (m_prefixPlan != 0) {
        cufftDestroy(m_prefixPlan);
        m_prefixPlan = 0;
    }
}

bool FFT::init() {
    if (m_fftSize == 0) {
        ERROR << "FFT::init: fft size must be greater than zero." << std::endl;
        return false;
    }

    if (m_overlapSize < 0) {
        ERROR << "FFT::init: overlap size must be non-negative." << std::endl;
        return false;
    }

    if (static_cast<size_t>(m_overlapSize) >= m_fftSize) {
        ERROR << "FFT::init: overlap size must be less than fft size." << std::endl;
        return false;
    }

    m_hopSize = m_fftSize - static_cast<size_t>(m_overlapSize);
    if (m_hopSize == 0) {
        ERROR << "FFT::init: hop size must be greater than zero." << std::endl;
        return false;
    }

    return true;
}

bool FFT::initPlan(){
    const auto inputSize = std::visit([](const auto& ptr) { return ptr->size(); }, m_inDataPtr);
    const auto batch = (inputSize < m_fftSize)
        ? 0
        : static_cast<int32_t>(1 + (inputSize - m_fftSize) / m_hopSize);
    if (batch <= 0) {
        ERROR << "FFT::initPlan: no FFT frames to process (input too small)." << std::endl;
        return false;
    }

    m_plan = 0;
    int n[1] = {static_cast<int>(m_fftSize)};
    const int istride = 1;
    const int ostride = 1;
    const int idist = static_cast<int>(m_hopSize);
    const int odist = static_cast<int>(m_outputPerBatch);

    const auto planStatus = cufftPlanMany(
        &m_plan,
        1,
        n,
        nullptr,
        istride,
        idist,
        nullptr,
        ostride,
        odist,
        m_planType,
        batch
    );
    if (planStatus != CUFFT_SUCCESS) {
        ERROR << "FFT::initPlan: cufftPlanMany failed: " << cufftResultToString(planStatus) << std::endl;
        return false;
    }

    return true;
}

bool FFT::initPrefixPlan() {
    m_prefixPlan = 0;

    int n[1] = {static_cast<int>(m_fftSize)};
    const auto planStatus = cufftPlanMany(
        &m_prefixPlan,
        1,
        n,
        nullptr,
        1,
        static_cast<int>(m_fftSize),
        nullptr,
        1,
        static_cast<int>(m_outputPerBatch),
        m_planType,
        1
    );
    if (planStatus != CUFFT_SUCCESS) {
        ERROR << "FFT::initPrefixPlan: cufftPlanMany failed: " << cufftResultToString(planStatus) << std::endl;
        return false;
    }
    return true;
}

bool FFT::saveInputTailToBuffer() {
    const auto overlap = static_cast<size_t>(m_overlapSize);
    if (overlap == 0) {
        return true;
    }

    const auto inputSize = std::visit([](const auto& ptr) { return ptr->size(); }, m_inDataPtr);
    if (inputSize < overlap) {
        ERROR << "FFT::saveInputTailToBuffer: input size is smaller than overlap size." << std::endl;
        return false;
    }

    if (m_inputKind == InputKind::Real) {
        const auto& inData = std::get<GpuFloatSignalPtr>(m_inDataPtr);
        return saveInputTailToBufferImpl(m_bufferReal, inData, overlap);
    }

    const auto& inData = std::get<GpuComplexFloatSignalPtr>(m_inDataPtr);
    return saveInputTailToBufferImpl(m_bufferComplex, inData, overlap);
}

bool FFT::executeStitchFft() {
    const auto overlap = static_cast<size_t>(m_overlapSize);
    if (m_isFirstFft || overlap == 0) {
        return true;
    }

    auto* outPtr = reinterpret_cast<cufftComplex*>(m_outData->getDeviceData());
    if (m_inputKind == InputKind::Real) {
        const auto& inData = std::get<GpuFloatSignalPtr>(m_inDataPtr);
        if (!preparePrefixInputImpl(m_prefixInputReal, m_bufferReal, inData, m_fftSize, overlap)) {
            return false;
        }
    } else {
        const auto& inData = std::get<GpuComplexFloatSignalPtr>(m_inDataPtr);
        if (!preparePrefixInputImpl(m_prefixInputComplex, m_bufferComplex, inData, m_fftSize, overlap)) {
            return false;
        }
    }

    if (!initPrefixPlan()) {
        return false;
    }

    cufftResult execStatus = CUFFT_SUCCESS;
    if (m_inputKind == InputKind::Real) {
        execStatus = cufftExecR2C(
            m_prefixPlan,
            m_prefixInputReal.getDeviceData(),
            outPtr
        );
    } else {
        execStatus = cufftExecC2C(
            m_prefixPlan,
            reinterpret_cast<cufftComplex*>(m_prefixInputComplex.getDeviceData()),
            outPtr,
            CUFFT_FORWARD
        );
    }
    if (execStatus != CUFFT_SUCCESS) {
        ERROR << "FFT::executeStitchFft: cufftExec failed: " << cufftResultToString(execStatus) << std::endl;
        return false;
    }

    return true;
}

bool FFT::run() {
    if (m_plan != 0) {
        cufftDestroy(m_plan);
        m_plan = 0;
    }
    if (m_prefixPlan != 0) {
        cufftDestroy(m_prefixPlan);
        m_prefixPlan = 0;
    }

    const auto inputSize = std::visit([](const auto& ptr) { return ptr->size(); }, m_inDataPtr);
    if (inputSize < m_fftSize) {
        ERROR << "FFT::run: input size is smaller than fft size." << std::endl;
        return false;
    }

    if (not initPlan()) {
        return false;
    }

    const size_t prefixBins = (!m_isFirstFft && m_overlapSize > 0) ? m_outputPerBatch : 0;
    auto* outPtr = reinterpret_cast<cufftComplex*>(m_outData->getDeviceData());

    if (prefixBins > 0 && !executeStitchFft()) {
        return false;
    }

    cufftResult execStatus = CUFFT_SUCCESS;
    if (m_inputKind == InputKind::Real) {
        const auto& inData = std::get<GpuFloatSignalPtr>(m_inDataPtr);
        execStatus = cufftExecR2C(
            m_plan,
            inData->getDeviceData(),
            outPtr + prefixBins
        );
    } else {
        const auto& inData = std::get<GpuComplexFloatSignalPtr>(m_inDataPtr);
        execStatus = cufftExecC2C(
            m_plan,
            reinterpret_cast<cufftComplex*>(inData->getDeviceData()),
            outPtr + prefixBins,
            CUFFT_FORWARD
        );
    }
    if (execStatus != CUFFT_SUCCESS) {
        ERROR << "FFT::run: cufftExec failed: " << cufftResultToString(execStatus) << std::endl;
        cufftDestroy(m_plan);
        m_plan = 0;
        if (m_prefixPlan != 0) {
            cufftDestroy(m_prefixPlan);
            m_prefixPlan = 0;
        }
        return false;
    }

    const auto cudaErr = cudaGetLastError();
    if (cudaErr != cudaSuccess) {
        ERROR << "FFT::run: CUDA execution error after cufftExecR2C: " << cudaGetErrorString(cudaErr) << std::endl;
        cufftDestroy(m_plan);
        m_plan = 0;
        if (m_prefixPlan != 0) {
            cufftDestroy(m_prefixPlan);
            m_prefixPlan = 0;
        }
        return false;
    }

    if (!saveInputTailToBuffer()) {
        cufftDestroy(m_plan);
        m_plan = 0;
        if (m_prefixPlan != 0) {
            cufftDestroy(m_prefixPlan);
            m_prefixPlan = 0;
        }
        return false;
    }

    m_isFirstFft = false;

    cufftDestroy(m_plan);
    m_plan = 0;
    if (m_prefixPlan != 0) {
        cufftDestroy(m_prefixPlan);
        m_prefixPlan = 0;
    }

    return true;
}

void FFT::setParam(const std::string& paramName, const std::any& value) {
    if(paramName == "fft size"){
        m_fftSize = std::any_cast<int32_t>(value);
    }

    if(paramName == "overlap size"){
        m_overlapSize = std::any_cast<int32_t>(value);
    }
}

bool FFT::setData(std::shared_ptr<IData> data) {
    auto gpuFloatData = std::dynamic_pointer_cast<GpuFloatSignal>(data);
    if (gpuFloatData) {
        m_inDataPtr = gpuFloatData;
        m_inputKind = InputKind::Real;
        m_planType = CUFFT_R2C;
        m_outputPerBatch = (m_fftSize / 2) + 1;
    } else {
        auto gpuComplexData = std::dynamic_pointer_cast<GpuComplexFloatSignal>(data);
        if (!gpuComplexData) {
            ERROR << "FFT::setData: input IData is not supported signal type." << std::endl;
            return false;
        }
        m_inDataPtr = gpuComplexData;
        m_inputKind = InputKind::Complex;
        m_planType = CUFFT_C2C;
        m_outputPerBatch = m_fftSize;
    }

    const auto inputSize = std::visit([](const auto& ptr) { return ptr->size(); }, m_inDataPtr);
    if (m_hopSize == 0) {
        ERROR << "FFT::setData: init() was not called or hop size is invalid." << std::endl;
        return false;
    }

    if (inputSize < m_fftSize) {
        ERROR << "FFT::setData: input size is smaller than fft size." << std::endl;
        return false;
    }

    const auto batch = 1 + (inputSize - m_fftSize) / m_hopSize;
    const auto hasStitchFrame = (!m_isFirstFft && m_overlapSize > 0) ? 1 : 0;
    const auto outputSize = m_outputPerBatch * (batch + hasStitchFrame);
    auto outData = std::make_shared<GpuComplexFloatSignal>(outputSize);
    if (!outData) {
        ERROR << "FFT::setData: failed to allocate output GPU buffer, size = " << outputSize << std::endl;
        return false;
    }

    m_outData = outData;
    return true;
}

std::shared_ptr<IData> FFT::getData() {
    return m_outData;
}
