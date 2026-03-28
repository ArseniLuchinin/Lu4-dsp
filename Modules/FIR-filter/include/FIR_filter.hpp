#ifndef FIR_FILTER_HPP
#define FIR_FILTER_HPP

#include <IModule.hpp>
#include <IData.hpp>
#include <IVirtualRX.hpp>

#include <GpuFloatSignal.hpp>
#include <GpuComplexSignal.hpp>

#include <cuComplex.h>

class FIRFilter : public IModule, public IVirtualRX { 
public:
    FIRFilter();

    bool init() override;
    bool run() override;

    bool setData(std::shared_ptr<IData> data) override;
    std::shared_ptr<IData> getData() override;

    void setParam(const std::string& paramName, const std::any& value) override;

private:
    enum class SignalType {
        None,
        Float,
        ComplexFloat
    };

    int   m_M;
    int   m_blockSize;
    SignalType m_signalType = SignalType::None;

    std::shared_ptr<IData> m_data;
    std::shared_ptr<GpuFloatSignal> m_floatData;
    std::shared_ptr<GpuComplexFloatSignal> m_complexData;

    std::shared_ptr<float> m_historyFloat;
    std::shared_ptr<float> m_nextHistoryFloat;

    std::shared_ptr<cuComplex> m_historyComplex;
    std::shared_ptr<cuComplex> m_nextHistoryComplex;
};

#endif
