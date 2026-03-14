#ifndef FFT_CUFFT_H
#define FFT_CUFFT_H

#include <IModule.hpp>
#include <GpuFloatSignal.hpp>
#include <GpuComplexSignal.hpp>

#include <cufft.h>

#include <memory>
#include <variant>

class FFT : public IModule {
    using GpuFloatSignalPtr = std::shared_ptr<GpuFloatSignal>;
    using GpuComplexFloatSignalPtr = std::shared_ptr<GpuComplexFloatSignal>;
    using SignalPtr = std::variant<GpuComplexFloatSignalPtr, GpuFloatSignalPtr>;
    enum class InputKind { Real, Complex };

public:
    FFT();
    ~FFT() override;

    bool init() override;


    bool run() override;

    void setParam(const std::string& paramName, const std::any& value) override;

    bool setData(std::shared_ptr<IData> data) override;
    std::shared_ptr<IData> getData() override;

private:
    bool initPlan();

    bool initPrefixPlan();
    bool saveInputTailToBuffer();
    bool executeStitchFft();

    SignalPtr m_inDataPtr;
    InputKind m_inputKind = InputKind::Real;

    cufftType m_planType = CUFFT_R2C;
    size_t m_outputPerBatch = 0;
    std::shared_ptr<GpuComplexFloatSignal> m_outData;

    cufftHandle m_plan = 0;

    cufftHandle m_prefixPlan = 0;
    GpuFloatSignal m_prefixInputReal;
    GpuFloatSignal m_bufferReal;
    GpuComplexFloatSignal m_prefixInputComplex;
    GpuComplexFloatSignal m_bufferComplex;
    bool m_isFirstFft = true;

    size_t m_fftSize = 1024;
    int32_t m_overlapSize = m_fftSize / 2;
};

#endif
