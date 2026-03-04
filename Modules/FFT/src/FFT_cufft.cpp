#include "FFT_cufft.hpp"

#include <module.hpp>
#include <utility>

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

bool FFT::init() {
    return true;
}

bool FFT::prepareData() {
    if (!m_inData || !m_outData) {
        ERROR << "FFT::run: data is not prepared. Call setData() first." << std::endl;
        return false;
    }

    if (!m_inData->isValid() || !m_outData->isValid()) {
        ERROR << "FFT::run: input or output data is invalid." << std::endl;
        return false;
    }

    auto* inPtr = m_inData->getDeviceData();
    auto* outPtr = reinterpret_cast<cufftComplex*>(m_outData->getDeviceData());
    if (!inPtr || !outPtr) {
        ERROR << "FFT::run: device pointers are null." << std::endl;
        return false;
    }

    return true;
}

bool FFT::initPlan(){
    const auto inputSize = m_inData->size();
    const auto batch = static_cast<int32_t>(inputSize / m_fftSize);
    if (batch <= 0) {
        ERROR << "FFT::initPlan: invalid batch count." << std::endl;
        return false;
    }

    /*
    if ((inputSize % m_fftSize) != 0) {
        ERROR << "FFT::initPlan: input size (" << inputSize
              << ") is not multiple of fft size (" << m_fftSize << ")." << std::endl;
        return false;
    }
    */

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

bool FFT::run() {
    if(not prepareData()) {
        return false;
    }

    if(not initPlan()){
        return false;
    }

    const auto execStatus = cufftExecR2C(m_plan, m_inData->getDeviceData(), m_outData->getDeviceData());
    if (execStatus != CUFFT_SUCCESS) {
        ERROR << "FFT::run: cufftExecR2C failed: " << cufftResultToString(execStatus) << std::endl;
        cufftDestroy(m_plan);
        m_outData = std::make_shared<GpuComplexFloatSignal>();
        return false;
    }

    const auto cudaErr = cudaGetLastError();
    if (cudaErr != cudaSuccess) {
        ERROR << "FFT::run: CUDA execution error after cufftExecR2C: " << cudaGetErrorString(cudaErr) << std::endl;
        cufftDestroy(m_plan);
        return false;
    }

    return true;
}

void FFT::setParam(const std::string& paramName, const std::any& value) {
    if(paramName == "fft size"){
        m_fftSize = std::any_cast<int32_t>(value);
    }
}

bool FFT::setData(std::shared_ptr<IData> data) {
    auto gpuData = std::dynamic_pointer_cast<GpuFloatSignal>(data);
    if (!gpuData) {
        ERROR << "FFT::setData: input IData is not GpuFloatSignal." << std::endl;
        return false;
    }

    if (!gpuData->isValid()) {
        ERROR << "FFT::setData: input data is invalid." << std::endl;
        return false;
    }

    if (m_fftSize == 0) {
        ERROR << "FFT::setData: fft size is zero." << std::endl;
        return false;
    }

    const auto inputSize = gpuData->size();
    if (inputSize == 0) {
        ERROR << "FFT::setData: input signal has zero size." << std::endl;
        return false;
    }

    if (inputSize < m_fftSize) {
        ERROR << "FFT::setData: input size (" << inputSize
              << ") is smaller than fft size (" << m_fftSize << ")." << std::endl;
        return false;
    }

    /*
    if ((inputSize % m_fftSize) != 0) {
        ERROR << "FFT::setData: input size (" << inputSize
              << ") is not multiple of fft size (" << m_fftSize << ")." << std::endl;
        return false;
    }
    */

    const auto batch = inputSize / m_fftSize;
    const auto outputPerBatch = (m_fftSize / 2) + 1;
    const auto outputSize = batch * outputPerBatch;
    auto outData = std::make_shared<GpuComplexFloatSignal>(outputSize);
    if (!outData || !outData->isValid()) {
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
