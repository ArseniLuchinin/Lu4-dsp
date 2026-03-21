#include "FFT_cufft.hpp"
#include <VariablesResolve.hpp>
#include <module.hpp>
#include <utility>
#include <iostream>

#include <cufft.h>
#include <cuda_runtime.h>


namespace {
const char* cufftResultToString(cufftResult result) {
    switch (result) {
    case CUFFT_SUCCESS:
        return "CUFFT_SUCCESS";
    case CUFFT_INVALID_PLAN:
        return "CUFFT_INVALID_PLAN";
    case CUFFT_ALLOC_FAILED:
        return "CUFFT_ALLOC_FAILED";
    case CUFFT_INVALID_TYPE:
        return "CUFFT_INVALID_TYPE";
    case CUFFT_INVALID_VALUE:
        return "CUFFT_INVALID_VALUE";
    case CUFFT_INTERNAL_ERROR:
        return "CUFFT_INTERNAL_ERROR";
    case CUFFT_EXEC_FAILED:
        return "CUFFT_EXEC_FAILED";
    case CUFFT_SETUP_FAILED:
        return "CUFFT_SETUP_FAILED";
    case CUFFT_INVALID_SIZE:
        return "CUFFT_INVALID_SIZE";
    case CUFFT_UNALIGNED_DATA:
        return "CUFFT_UNALIGNED_DATA";
    default:
        return "CUFFT_UNKNOWN_ERROR";
    }
}
}

IModule* createModule() {
    return new FFT();
}

FFT::FFT() : IModule({"FFT", "FFT-module.so", "FFT.json"}) {}
FFT::~FFT() {
    if (m_plan != 0) {
        cufftDestroy(m_plan);
        m_plan = 0;
    }
}

bool FFT::init() {
    if (m_fftSize == 0) {
        ERROR << "FFT::init: fft size must be greater than zero." << std::endl;
        return false;
    }

    m_hopSize = m_fftSize;
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
    const int idist = static_cast<int>(m_fftSize);
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

bool FFT::run() {
    if (m_plan != 0) {
        cufftDestroy(m_plan);
        m_plan = 0;
    }

    const auto inputSize = std::visit([](const auto& ptr) { return ptr->size(); }, m_inDataPtr);
    if (inputSize < m_fftSize) {
        ERROR << "FFT::run: input size is smaller than fft size." << std::endl;
        return false;
    }

    if (not initPlan()) {
        return false;
    }

    auto* outPtr = reinterpret_cast<cufftComplex*>(m_outData->getDeviceData());

    cufftResult execStatus = CUFFT_SUCCESS;
    if (m_inputKind == InputKind::Real) {
        const auto& inData = std::get<GpuFloatSignalPtr>(m_inDataPtr);
        execStatus = cufftExecR2C(
            m_plan,
            inData->getDeviceData(),
            outPtr
        );
    } else {
        const auto& inData = std::get<GpuComplexFloatSignalPtr>(m_inDataPtr);
        execStatus = cufftExecC2C(
            m_plan,
            reinterpret_cast<cufftComplex*>(inData->getDeviceData()),
            outPtr,
            CUFFT_FORWARD
        );
    }
    if (execStatus != CUFFT_SUCCESS) {
        ERROR << "FFT::run: cufftExec failed: " << cufftResultToString(execStatus) << std::endl;
        cufftDestroy(m_plan);
        m_plan = 0;
        return false;
    }

    const auto cudaErr = cudaGetLastError();
    if (cudaErr != cudaSuccess) {
        ERROR << "FFT::run: CUDA execution error after cufftExecR2C: " << cudaGetErrorString(cudaErr) << std::endl;
        cufftDestroy(m_plan);
        m_plan = 0;
        return false;
    }

    cufftDestroy(m_plan);
    m_plan = 0;
    return true;
}

void FFT::setParam(const std::string& paramName, const std::any& value) {
    const std::any resolved = resolveParamValue(value);
    if(paramName == "fft size"){
        m_fftSize = std::any_cast<int32_t>(resolved);
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
    const auto outputSize = m_outputPerBatch * batch;
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
