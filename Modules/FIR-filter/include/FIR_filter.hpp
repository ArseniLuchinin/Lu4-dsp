#ifndef FIR_FILTER_HPP
#define FIR_FILTER_HPP

#include <IModule.hpp>
#include <IData.hpp>
#include <IVirtualRX.hpp>

#include <GpuFloatSignal.hpp>

class FIRFilter : public IModule, public IVirtualRX { 
public:
    FIRFilter();

    bool init() override;
    bool run() override;

    bool setData(std::shared_ptr<IData> data) override;
    std::shared_ptr<IData> getData() override;

    void setParam(const std::string& paramName, const std::any& value) override;

private:
    int   m_M;
    int   m_blockSize;

    std::shared_ptr<GpuFloatSignal> m_data;
    std::shared_ptr<float> m_history;
    std::shared_ptr<float> m_next_history;
};

#endif
