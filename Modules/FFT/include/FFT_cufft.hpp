#ifndef FFT_CUFFT_H
#define FFT_CUFFT_H

#include <IModule.hpp>
#include <GpuFloatSignal.hpp>

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
    std::shared_ptr<GpuFloatSignal> m_data;
};

#endif
