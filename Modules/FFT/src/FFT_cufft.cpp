#include "FFT_cufft.hpp"
#include "FFT_overlap_callback.hpp"
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

    if (m_hopSize == 0) {
        m_hopSize = m_fftSize;
    }
    if (m_hopSize == 0) {
        ERROR << "FFT::init: hop size must be greater than zero." << std::endl;
        return false;
    }

    if (m_hopSize > m_fftSize) {
        ERROR << "FFT::init: hop size must be less than or equal to fft size." << std::endl;
        return false;
    }

    m_overlapSize = m_fftSize - m_hopSize;
    m_overlapBuffer = std::make_shared<GpuComplexFloatSignal>(m_overlapSize);
    if (m_overlapSize > 0 && (!m_overlapBuffer || !m_overlapBuffer->isValid())) {
        ERROR << "FFT::init: failed to allocate overlap buffer." << std::endl;
        return false;
    }

    m_isFirstRun = true;
    return true;
}

int FFT::calcBatchCount(size_t inputSize) const {
    if (m_hopSize == 0) {
        return 0;
    }

    if (m_isFirstRun) {
        if (inputSize < m_overlapSize) {
            return 0;
        }

        return static_cast<int>((inputSize - m_overlapSize) / m_hopSize);
    }

    return static_cast<int>(inputSize / m_hopSize);
}

bool FFT::initPlan(int batchCount){
    if (batchCount <= 0) {
        ERROR << "FFT::initPlan: no FFT frames to process (input too small)." << std::endl;
        return false;
    }

    m_plan = 0;
    int n[1] = {static_cast<int>(m_fftSize)};
    const int istride = 1;
    const int ostride = 1;
    const int idist = static_cast<int>(m_fftSize);
    const int odist = static_cast<int>(m_outputPerBatch);
    size_t workSize = 0;

    const auto createStatus = cufftCreate(&m_plan);
    if (createStatus != CUFFT_SUCCESS) {
        ERROR << "FFT::initPlan: cufftCreate failed: "
              << cufftResultToString(createStatus) << std::endl;
        m_plan = 0;
        return false;
    }

    const auto planStatus = cufftMakePlanMany(
        m_plan,
        1,
        n,
        nullptr,
        istride,
        idist,
        nullptr,
        ostride,
        odist,
        CUFFT_C2C,
        batchCount,
        &workSize
    );
    if (planStatus != CUFFT_SUCCESS) {
        ERROR << "FFT::initPlan: cufftPlanMany failed: " << cufftResultToString(planStatus) << std::endl;
        cufftDestroy(m_plan);
        m_plan = 0;
        return false;
    }

    return true;
}

bool FFT::updateOverlapBuffer(cufftComplex* inputPtr, int batchCount) {
    if (m_overlapSize == 0) {
        return true;
    }

    const size_t tailStart = m_isFirstRun
        ? static_cast<size_t>(batchCount) * m_hopSize
        : static_cast<size_t>(batchCount) * m_hopSize - m_overlapSize;

    m_overlapBuffer->setDataFromDevice(
        reinterpret_cast<cuComplex*>(inputPtr + tailStart),
        m_overlapSize
    );
    if (!m_overlapBuffer->isValid()) {
        ERROR << "FFT::updateOverlapBuffer: failed to update overlap buffer." << std::endl;
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

    if (m_inputKind != InputKind::Complex) {
        ERROR << "FFT::run: overlap implementation is currently available only for complex input." << std::endl;
        return false;
    }

    const int batchCount = calcBatchCount(inputSize);
    if (batchCount <= 0) {
        ERROR << "FFT::run: no FFT frames to process." << std::endl;
        return false;
    }

    if (not initPlan(batchCount)) {
        return false;
    }

    auto* outPtr = reinterpret_cast<cufftComplex*>(m_outData->getDeviceData());
    const auto& inData = std::get<GpuComplexFloatSignalPtr>(m_inDataPtr);
    auto* inPtr = reinterpret_cast<cufftComplex*>(inData->getDeviceData());
    auto* signalPtr = m_isFirstRun ? (inPtr + m_overlapSize) : inPtr;

    if (m_isFirstRun && m_overlapSize > 0) {
        m_overlapBuffer->setDataFromDevice(reinterpret_cast<cuComplex*>(inPtr), m_overlapSize);
        if (!m_overlapBuffer->isValid()) {
            ERROR << "FFT::run: failed to initialize overlap buffer." << std::endl;
            cufftDestroy(m_plan);
            m_plan = 0;
            return false;
        }
    }

    void* callbackData = nullptr;
    std::string callbackError;
    if (!setupOverlapLoadCallback(
            m_plan,
            signalPtr,
            reinterpret_cast<cufftComplex*>(m_overlapBuffer->getDeviceData()),
            static_cast<int>(m_fftSize),
            static_cast<int>(m_overlapSize),
            static_cast<int>(m_hopSize),
            &callbackData,
            &callbackError)) {
        ERROR << "FFT::run: failed to configure overlap callback. "
              << callbackError << std::endl;
        cufftDestroy(m_plan);
        m_plan = 0;
        return false;
    }

    const auto execStatus = cufftExecC2C(
        m_plan,
        inPtr,
        outPtr,
        CUFFT_FORWARD
    );
    if (execStatus != CUFFT_SUCCESS) {
        ERROR << "FFT::run: cufftExec failed: " << cufftResultToString(execStatus) << std::endl;
        releaseOverlapLoadCallback(callbackData);
        cufftDestroy(m_plan);
        m_plan = 0;
        return false;
    }

    const auto cudaErr = cudaGetLastError();
    if (cudaErr != cudaSuccess) {
        ERROR << "FFT::run: CUDA execution error after cufftExecR2C: " << cudaGetErrorString(cudaErr) << std::endl;
        releaseOverlapLoadCallback(callbackData);
        cufftDestroy(m_plan);
        m_plan = 0;
        return false;
    }

    if (!updateOverlapBuffer(inPtr, batchCount)) {
        releaseOverlapLoadCallback(callbackData);
        cufftDestroy(m_plan);
        m_plan = 0;
        return false;
    }

    releaseOverlapLoadCallback(callbackData);
    cufftDestroy(m_plan);
    m_plan = 0;
    m_isFirstRun = false;
    return true;
}

void FFT::setParam(const std::string& paramName, const std::any& value) {
    const std::any resolved = resolveParamValue(value);
    if(paramName == "fft size"){
        m_fftSize = std::any_cast<int32_t>(resolved);
    }

    if (paramName == "hop size") {
        m_hopSize = std::any_cast<int32_t>(resolved);
    }
}

bool FFT::setData(std::shared_ptr<IData> data) {
    auto gpuFloatData = std::dynamic_pointer_cast<GpuFloatSignal>(data);
    if (gpuFloatData) {
        ERROR << "FFT::setData: real input is temporarily disabled while overlap logic is implemented only for complex signals." << std::endl;
        return false;
    }

    auto gpuComplexData = std::dynamic_pointer_cast<GpuComplexFloatSignal>(data);
    if (!gpuComplexData) {
        ERROR << "FFT::setData: input IData is not supported signal type." << std::endl;
        return false;
    }

    m_inDataPtr = gpuComplexData;
    m_inputKind = InputKind::Complex;
    m_planType = CUFFT_C2C;
    m_outputPerBatch = m_fftSize;

    const auto inputSize = std::visit([](const auto& ptr) { return ptr->size(); }, m_inDataPtr);
    if (m_hopSize == 0) {
        ERROR << "FFT::setData: init() was not called or hop size is invalid." << std::endl;
        return false;
    }

    if (inputSize < m_fftSize) {
        ERROR << "FFT::setData: input size is smaller than fft size." << std::endl;
        return false;
    }

    const auto batch = calcBatchCount(inputSize);
    if (batch <= 0) {
        ERROR << "FFT::setData: no FFT frames to process for current input." << std::endl;
        return false;
    }

    const auto outputSize = m_outputPerBatch * static_cast<size_t>(batch);
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
