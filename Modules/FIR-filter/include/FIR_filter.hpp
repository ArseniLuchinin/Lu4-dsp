#ifndef FIR_FILTER_HPP
#define FIR_FILTER_HPP

#include <IModule.hpp>
#include <IData.hpp>

#include <GpuFloatSignal.hpp>

class FIRFilter : public IModule {
public:
    FIRFilter();

    bool init() override;
    bool run() override;

    bool setData(std::shared_ptr<IData> data) override;
    std::shared_ptr<IData> getData() override;

    void setParam(const std::string& paramName, const std::any& value) override;

private:
    float m_Fs;
    float m_F1;
    float m_F2;
    int   m_M;
    int   m_blockSize;

    std::vector<float> m_coeff;

    std::shared_ptr<GpuFloatSignal> m_data;
    std::shared_ptr<float> m_history;
    std::shared_ptr<float> m_next_history;
};

#endif