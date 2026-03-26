#ifndef FFT_SHIFT_HPP
#define FFT_SHIFT_HPP

#include <IModule.hpp>
#include <GpuFloatSignal.hpp>

#include <memory>

class FFT_Shift : public IModule {
public:
    FFT_Shift();
    ~FFT_Shift() override = default;

    bool init() override;
    bool run() override;

    void setParam(const std::string& paramName, const std::any& value) override;

    bool setData(std::shared_ptr<IData> data) override;
    std::shared_ptr<IData> getData() override;

private:
    size_t m_fftSize = 1024;
    size_t m_windowSize = 0;

    std::shared_ptr<GpuFloatSignal> m_inData;
    std::shared_ptr<GpuFloatSignal> m_outData;
};

#endif
