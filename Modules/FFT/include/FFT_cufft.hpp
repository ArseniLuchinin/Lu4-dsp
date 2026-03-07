#ifndef FFT_CUFFT_H
#define FFT_CUFFT_H

#include <IModule.hpp>
#include <GpuFloatSignal.hpp>
#include <GpuComplexSignal.hpp>

#include <cufft.h>

#include <memory>

class FFT : public IModule {
public:
    FFT();
    ~FFT() override = default;

    bool init() override;
    bool run() override;

    void setParam(const std::string& paramName, const std::any& value) override;

    bool setData(std::shared_ptr<IData> data) override;
    std::shared_ptr<IData> getData() override;

private:
    bool prepareData();
    bool initPlan();

    std::shared_ptr<GpuFloatSignal> m_inData;
    std::shared_ptr<GpuComplexFloatSignal> m_outData;

    cufftHandle m_plan;

    cufftHandle m_prefixPlan;
    GpuFloatSignal m_buffer;
    bool m_isFirtFft = true;

    size_t m_fftSize = 1024;
    int32_t m_overlapSize = m_fftSize / 2;
};

#endif
