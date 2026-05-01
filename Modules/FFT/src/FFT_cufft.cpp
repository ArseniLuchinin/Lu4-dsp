#include "FFT_cufft.hpp"

#include <VariablesResolve.hpp>
#include <module.hpp>

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

} // namespace

IModule* createModule() {
    return new FFT();
}

FFT::FFT() : IModule({"FFT", "libFFT-module.so", "module.json"}) {}

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
    m_batchCount = 0;
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
    m_plan = 0;
    int n[1] = {static_cast<int>(m_fftSize)};
    const int istride = 1;
    const int ostride = 1;
    const int idist = static_cast<int>(m_fftSize);
    const int odist = static_cast<int>(m_impl->outputPerBatch(m_fftSize));
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
        m_impl->planType(),
        batchCount,
        &workSize
    );
    if (planStatus != CUFFT_SUCCESS) {
        ERROR << "FFT::initPlan: cufftMakePlanMany failed: "
              << cufftResultToString(planStatus) << std::endl;
        cufftDestroy(m_plan);
        m_plan = 0;
        return false;
    }

    return true;
}

bool FFT::run() {
    if (m_plan != 0) {
        cufftDestroy(m_plan);
        m_plan = 0;
    }

    if (!initPlan(m_batchCount)) {
        return false;
    }

    if (!m_impl->execute(m_plan, m_fftSize, m_hopSize, m_overlapSize, m_isFirstRun, m_batchCount)) {
        ERROR << "FFT::run: implementation execute failed. "
              << m_impl->lastError() << std::endl;
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
    if(paramName == "fft size"){
        m_fftSize = std::any_cast<int64_t>(value);
    }

    if (paramName == "hop size") {
        m_hopSize = std::any_cast<int64_t>(value);
    }
}

bool FFT::setData(std::shared_ptr<IData> data) {
    if (std::dynamic_pointer_cast<GpuFloatSignal>(data)) {
        if (!m_impl) {
            m_impl = std::make_unique<RealFFTOverlapImpl>();
        } else if (dynamic_cast<RealFFTOverlapImpl*>(m_impl.get()) == nullptr) {
            ERROR << "FFT::setData: implementation type changed unexpectedly." << std::endl;
            return false;
        }
    } else if (std::dynamic_pointer_cast<GpuComplexFloatSignal>(data)) {
        if (!m_impl) {
            m_impl = std::make_unique<ComplexFFTOverlapImpl>();
        } else if (dynamic_cast<ComplexFFTOverlapImpl*>(m_impl.get()) == nullptr) {
            ERROR << "FFT::setData: implementation type changed unexpectedly." << std::endl;
            return false;
        }
    } else {
        ERROR << "FFT::setData: input IData is not supported signal type." << std::endl;
        return false;
    }

    const auto inputSize = data->size();
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

    if (!m_impl->setData(data)) {
        ERROR << "FFT::setData: implementation rejected input. "
              << m_impl->lastError() << std::endl;
        return false;
    }

    if (!m_impl->ensureOutputForBatch(batch, m_fftSize)) {
        ERROR << "FFT::setData: failed to allocate output buffer. "
              << m_impl->lastError() << std::endl;
        return false;
    }

    m_batchCount = batch;
    return true;
}

std::shared_ptr<IData> FFT::getData() {
    return m_impl ? m_impl->getData() : nullptr;
}
