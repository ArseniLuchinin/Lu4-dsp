#include "FFT_cufft.hpp"
#include "FFT_overlap_callback.hpp"
#include <VariablesResolve.hpp>
#include <type_traits>
#include <module.hpp>
#include <utility>
#include <iostream>

#include <cufft.h>
#include <cuda_runtime.h>


namespace {

template<typename T>
constexpr bool is_complex_v = std::is_same_v<T, cufftComplex>;

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
        m_planType,
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

bool FFT::updateOverlapBuffer(float* inputPtr, int batchCount, const GpuFloatSignalPtr& buffer) {
    if (m_overlapSize == 0) {
        return true;
    }

    const size_t tailStart = m_isFirstRun
        ? static_cast<size_t>(batchCount) * m_hopSize
        : static_cast<size_t>(batchCount) * m_hopSize - m_overlapSize;

    buffer->setDataFromDevice(
        inputPtr + tailStart,
        m_overlapSize
    );
    if (!buffer->isValid()) {
        ERROR << "FFT::updateOverlapBuffer: failed to update overlap buffer." << std::endl;
        return false;
    }

    return true;
}

bool FFT::updateOverlapBuffer(cufftComplex* inputPtr, int batchCount, const GpuComplexFloatSignalPtr& buffer) {
    if (m_overlapSize == 0) {
        return true;
    }

    const size_t tailStart = m_isFirstRun
        ? static_cast<size_t>(batchCount) * m_hopSize
        : static_cast<size_t>(batchCount) * m_hopSize - m_overlapSize;

    buffer->setDataFromDevice(
        reinterpret_cast<cuComplex*>(inputPtr + tailStart),
        m_overlapSize
    );
    if (!buffer->isValid()) {
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
    const auto runVisitor = [&](const auto& inData, const auto& overlapBuffer) -> bool {
        using InPtrType = std::decay_t<decltype(inData)>;
        using BufferPtrType = std::decay_t<decltype(overlapBuffer)>;
        using SampleType = std::conditional_t<
            std::is_same_v<InPtrType, GpuComplexFloatSignalPtr>,
            cufftComplex,
            float>;

        if constexpr (!std::is_same_v<InPtrType, BufferPtrType>) {
            ERROR << "FFT::run: overlap buffer type does not match input type." << std::endl;
            return false;
        } else {
            auto* inPtr = reinterpret_cast<SampleType*>(inData->getDeviceData());
            auto* signalPtr = m_isFirstRun ? (inPtr + m_overlapSize) : inPtr;

            if (m_isFirstRun && m_overlapSize > 0) {
                if constexpr (is_complex_v<SampleType>) {
                    overlapBuffer->setDataFromDevice(reinterpret_cast<cuComplex*>(inPtr), m_overlapSize);
                } else {
                    overlapBuffer->setDataFromDevice(inPtr, m_overlapSize);
                }

                if (!overlapBuffer->isValid()) {
                    ERROR << "FFT::run: failed to initialize overlap buffer." << std::endl;
                    return false;
                }
            }

            void* callbackData = nullptr;
            std::string callbackError;
            SampleType* overlapPtr = reinterpret_cast<SampleType*>(overlapBuffer->getDeviceData());
            if (!setupOverlapLoadCallback(
                    m_plan,
                    signalPtr,
                    overlapPtr,
                    static_cast<int>(m_fftSize),
                    static_cast<int>(m_overlapSize),
                    static_cast<int>(m_hopSize),
                    &callbackData,
                    &callbackError)) {
                ERROR << "FFT::run: failed to configure overlap callback. "
                      << callbackError << std::endl;
                return false;
            }

            cufftResult execStatus = CUFFT_SUCCESS;
            if constexpr (is_complex_v<SampleType>) {
                execStatus = cufftExecC2C(m_plan,
                                          reinterpret_cast<cufftComplex*>(inPtr),
                                          outPtr,
                                          CUFFT_FORWARD);
            } else {
                execStatus = cufftExecR2C(m_plan, inPtr, outPtr);
            }

            if (execStatus != CUFFT_SUCCESS) {
                ERROR << "FFT::run: cufftExec failed: "
                      << cufftResultToString(execStatus) << std::endl;
                releaseOverlapLoadCallback(callbackData);
                return false;
            }

            const auto cudaErr = cudaGetLastError();
            if (cudaErr != cudaSuccess) {
                ERROR << "FFT::run: CUDA execution error after cufftExec: "
                      << cudaGetErrorString(cudaErr) << std::endl;
                releaseOverlapLoadCallback(callbackData);
                return false;
            }

            if (!updateOverlapBuffer(inPtr, batchCount, overlapBuffer)) {
                releaseOverlapLoadCallback(callbackData);
                return false;
            }

            releaseOverlapLoadCallback(callbackData);
            return true;
        }
    };

    const bool runOk = std::visit(runVisitor, m_inDataPtr, m_overlapBuffer);
    if (!runOk) {
        cufftDestroy(m_plan);
        m_plan = 0;
        return false;
    }

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
        m_inDataPtr = gpuFloatData;
        m_inputKind = InputKind::Real;
        m_planType = CUFFT_R2C;
        m_outputPerBatch = (m_fftSize / 2) + 1;

        const auto* complexBuffer = std::get_if<GpuComplexFloatSignalPtr>(&m_overlapBuffer);
        const auto* floatBuffer = std::get_if<GpuFloatSignalPtr>(&m_overlapBuffer);

        if (floatBuffer && *floatBuffer) {
            // штатный путь: переиспользуем существующий overlap-buffer
        } else if (complexBuffer && *complexBuffer) {
            ERROR << "FFT::setData: overlap buffer type changed unexpectedly." << std::endl;
            return false;
        } else {
            m_overlapBuffer = std::make_shared<GpuFloatSignal>(m_overlapSize);
        }
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

        const auto* complexBuffer = std::get_if<GpuComplexFloatSignalPtr>(&m_overlapBuffer);
        const auto* floatBuffer = std::get_if<GpuFloatSignalPtr>(&m_overlapBuffer);

        if (complexBuffer && *complexBuffer) {
            // штатный путь: переиспользуем существующий overlap-buffer
        } else if (floatBuffer && *floatBuffer) {
            ERROR << "FFT::setData: overlap buffer type changed unexpectedly." << std::endl;
            return false;
        } else {
            m_overlapBuffer = std::make_shared<GpuComplexFloatSignal>(m_overlapSize);
        }
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

    const bool overlapOk = std::visit([](const auto& ptr) {
        return ptr != nullptr && ptr->isValid();
    }, m_overlapBuffer);
    if (m_overlapSize > 0 && !overlapOk) {
        ERROR << "FFT::setData: failed to allocate overlap buffer." << std::endl;
        return false;
    }

    m_outData = outData;
    return true;
}

std::shared_ptr<IData> FFT::getData() {
    return m_outData;
}
