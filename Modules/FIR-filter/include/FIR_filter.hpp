#ifndef FIR_FILTER_HPP
#define FIR_FILTER_HPP

#include <IData.hpp>
#include <IGpuSignalData.hpp>
#include <IModule.hpp>
#include <IVirtualRX.hpp>

#include <GpuComplexSignal.hpp>
#include <GpuFloatSignal.hpp>

#include <cuComplex.h>

class FIRFilter : public IModule {
public:
  FIRFilter();
  ~FIRFilter() override;

  bool init() override;
  bool run() override;

  bool setData(std::shared_ptr<IData> data) override;
  std::shared_ptr<IData> getData() override;

  void setParam(const std::string &paramName, const std::any &value) override;

private:
  enum class CoefficientsTypeMode { Auto, Real, Complex };

  int m_M = 0;
  bool m_logEnergy = true;
  CoefficientsTypeMode m_coefficientsTypeMode = CoefficientsTypeMode::Auto;

  std::shared_ptr<IData> m_data;
  std::shared_ptr<IGpuSignalData> m_gpuData;
  std::shared_ptr<IGpuSignalData> m_taps;
  std::shared_ptr<IGpuSignalData> m_workData;
  std::shared_ptr<IGpuSignalData> m_historyData;
  std::shared_ptr<IGpuSignalData> m_nextHistoryData;
  double *m_energyBuffer = nullptr;
};

#endif
