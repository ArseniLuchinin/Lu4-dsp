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

    bool init() override;
    bool run() override;

    bool setData(std::shared_ptr<IData> data) override;
    std::shared_ptr<IData> getData() override;

    void setParam(const std::string& paramName, const std::any& value) override;

private:
    int   m_M = 0;
    int   m_blockSize = 0;

    std::shared_ptr<IData> m_data;
    std::shared_ptr<IGpuSignalData> m_gpuData;
    std::shared_ptr<IGpuSignalData> m_historyData;
    std::shared_ptr<IGpuSignalData> m_nextHistoryData;
};

#endif
