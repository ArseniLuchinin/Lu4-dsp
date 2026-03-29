#ifndef FIR_FILTER_HPP
#define FIR_FILTER_HPP

#include <IModule.hpp>
#include <IData.hpp>
#include <IVirtualRX.hpp>
#include <IGpuSignalData.hpp>

#include <GpuFloatSignal.hpp>
#include <GpuComplexSignal.hpp>

#include <cuComplex.h>

class FIRFilter : public IModule, public IVirtualRX { 
public:
    FIRFilter();
    ~FIRFilter() override;

    bool init() override;
    bool run() override;

    bool setData(std::shared_ptr<IData> data) override;
    std::shared_ptr<IData> getData() override;

    void setParam(const std::string& paramName, const std::any& value) override;

private:
    enum class TapType {
        None,
        Real,
        Complex
    };

    enum class CoefficientsTypeMode {
        Auto,
        Real,
        Complex
    };

    int   m_M = 0;
    bool m_logEnergy = true;
    TapType m_tapType = TapType::None;
    CoefficientsTypeMode m_coefficientsTypeMode = CoefficientsTypeMode::Auto;

    std::shared_ptr<IData> m_data;
    std::shared_ptr<IGpuSignalData> m_gpuData;
    std::shared_ptr<IGpuSignalData> m_workData;
    std::shared_ptr<IGpuSignalData> m_historyData;
    std::shared_ptr<IGpuSignalData> m_nextHistoryData;
    std::shared_ptr<GpuFloatSignal> m_realTaps;
    std::shared_ptr<GpuComplexFloatSignal> m_complexTaps;
    double* m_energyBuffer = nullptr;
};

#endif
