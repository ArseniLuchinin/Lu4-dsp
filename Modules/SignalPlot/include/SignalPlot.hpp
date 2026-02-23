#ifndef SIGNAL_PLOT_H
#define SIGNAL_PLOT_H

#include <IModule.hpp>
#include <CpuFloatSignal.hpp>
#include <any>

class SignalPlot : public IModule
{
public:
    SignalPlot();
    virtual ~SignalPlot();
    virtual bool init() override;
    virtual bool run() override;

    virtual void setParam(const std::string& paramName, const std::any& value) override;

    virtual bool setData(std::shared_ptr<IData> data) override;
    virtual std::shared_ptr<IData> getData() override;
protected:
    std::shared_ptr<CpuFloatSignal> m_data;
    size_t m_samplateRate = 1;

    std::string m_savePath;
    bool m_isShow = true;
};

#endif
