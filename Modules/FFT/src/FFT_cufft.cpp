#include "FFT_cufft.hpp"

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

    if (m_prefixPlan != 0) {
        cufftDestroy(m_prefixPlan);
        m_prefixPlan = 0;
    }
}

bool FFT::init() {
    return true;
}

bool FFT::initPlan(){
    const auto inputSize = m_inData->size();
    const auto batch = static_cast<int32_t>(inputSize / m_fftSize);

    m_plan = 0;
    int n[1] = {static_cast<int>(m_fftSize)};
    const int istride = 1;
    const int ostride = 1;
    const int idist = static_cast<int>(m_fftSize);
    const int odist = static_cast<int>((m_fftSize / 2) + 1);

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
        CUFFT_R2C,
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
        static_cast<int>((m_fftSize / 2) + 1),
        CUFFT_R2C,
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

    if (!m_buffer.reserve(overlap)) {
        ERROR << "FFT::saveInputTailToBuffer: failed to reserve buffer." << std::endl;
        return false;
    }

    auto* inPtr = m_inData->getDeviceData();
    auto* bufferPtr = m_buffer.getDeviceData();

    m_buffer.setDataFromDevice(inPtr + (m_inData->size() - overlap), overlap);

    return true;
}

bool FFT::executeStitchFft() {
    const auto overlap = static_cast<size_t>(m_overlapSize);
    if (m_isFirstFft || overlap == 0) {
        return true;
    }

    const size_t headSize = m_fftSize - overlap;
    if (!m_prefixInput.reserve(m_fftSize)) {
        ERROR << "FFT::executeStitchFft: failed to reserve prefix input." << std::endl;
        return false;
    }

    auto* prefixPtr = m_prefixInput.getDeviceData();
    auto* inPtr = m_inData->getDeviceData();
    auto* bufferPtr = m_buffer.getDeviceData();
    auto* outPtr = reinterpret_cast<cufftComplex*>(m_outData->getDeviceData());

    const auto copyTail = cudaMemcpy(
        prefixPtr,
        bufferPtr,
        overlap * sizeof(float),
        cudaMemcpyDeviceToDevice
    );
    if (copyTail != cudaSuccess) {
        ERROR << "FFT::executeStitchFft: tail copy failed: " << cudaGetErrorString(copyTail) << std::endl;
        return false;
    }

    const auto copyHead = cudaMemcpy(
        prefixPtr + overlap,
        inPtr,
        headSize * sizeof(float),
        cudaMemcpyDeviceToDevice
    );
    if (copyHead != cudaSuccess) {
        ERROR << "FFT::executeStitchFft: head copy failed: " << cudaGetErrorString(copyHead) << std::endl;
        return false;
    }

    if (!initPrefixPlan()) {
        return false;
    }

    const auto execStatus = cufftExecR2C(
        m_prefixPlan,
        m_prefixInput.getDeviceData(),
        outPtr
    );
    if (execStatus != CUFFT_SUCCESS) {
        ERROR << "FFT::executeStitchFft: cufftExecR2C failed: " << cufftResultToString(execStatus) << std::endl;
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

    if (not initPlan()) {
        return false;
    }

    const size_t prefixBins = (!m_isFirstFft && m_overlapSize > 0) ? ((m_fftSize / 2) + 1) : 0;
    auto* outPtr = reinterpret_cast<cufftComplex*>(m_outData->getDeviceData());

    if (prefixBins > 0 && !executeStitchFft()) {
        return false;
    }

    const auto execStatus = cufftExecR2C(
        m_plan,
        m_inData->getDeviceData(),
        outPtr + prefixBins
    );
    if (execStatus != CUFFT_SUCCESS) {
        ERROR << "FFT::run: cufftExecR2C failed: " << cufftResultToString(execStatus) << std::endl;
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
    auto gpuData = std::dynamic_pointer_cast<GpuFloatSignal>(data);
    if (!gpuData) {
        ERROR << "FFT::setData: input IData is not GpuFloatSignal." << std::endl;
        return false;
    }

    const auto inputSize = gpuData->size();

    const auto batch = inputSize / m_fftSize;
    const auto outputPerBatch = (m_fftSize / 2) + 1;
    const auto hasStitchFrame = (!m_isFirstFft && m_overlapSize > 0) ? 1 : 0;
    const auto outputSize = outputPerBatch * (batch + hasStitchFrame);
    auto outData = std::make_shared<GpuComplexFloatSignal>(outputSize);
    if (!outData) {
        ERROR << "FFT::setData: failed to allocate output GPU buffer, size = " << outputSize << std::endl;
        return false;
    }

    m_inData = gpuData;
    m_outData = outData;
    return true;
}

std::shared_ptr<IData> FFT::getData() {
    return m_outData;
}
