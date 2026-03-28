#ifndef FFT_CUFFT_H
#define FFT_CUFFT_H

#include <IModule.hpp>
#include <FFT_overlap_impl.hpp>

#include <cufft.h>

#include <memory>

class FFT : public IModule {
public:
    FFT();
    ~FFT() override;

    bool init() override;
    bool run() override;

    void setParam(const std::string& paramName, const std::any& value) override;

    bool setData(std::shared_ptr<IData> data) override;
    std::shared_ptr<IData> getData() override;

private:
    bool initPlan(int batchCount);
    int calcBatchCount(size_t inputSize) const;

    std::unique_ptr<IFFTOverlapImpl> m_impl;
    cufftHandle m_plan = 0;

    size_t m_fftSize = 1024;
    size_t m_hopSize = 0;
    size_t m_overlapSize = 0;
    int m_batchCount = 0;
    bool m_isFirstRun = true;
};

#endif
