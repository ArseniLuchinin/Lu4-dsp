#ifndef SIGNAL_PLOT__H
#define SIGNAL_PLOT__H

#include <matplot/matplot.h>

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
    size_t m_samplateRate = 1;

    /// @brief  Данные с которыми работаем
    matplot::vector_1d m_data;
    /// @brief Изначальные данне, передаются дальше без изменений
    std::shared_ptr<IData> m_transitData;

    std::string m_savePath;
    bool m_isShow = true;
};

#endif
