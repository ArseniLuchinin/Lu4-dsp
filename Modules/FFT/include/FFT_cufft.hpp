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

    SignalPtr m_inDataPtr;
    InputKind m_inputKind = InputKind::Real;

    cufftType m_planType = CUFFT_R2C;
    size_t m_outputPerBatch = 0;
    std::shared_ptr<GpuComplexFloatSignal> m_outData;

    cufftHandle m_plan = 0;

    size_t m_fftSize = 1024;
    size_t m_hopSize = 0;
};

#endif
