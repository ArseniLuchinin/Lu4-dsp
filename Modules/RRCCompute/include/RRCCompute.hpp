#ifndef RRC_COMPUTE_HPP
#define RRC_COMPUTE_HPP

#include <GpuFloatSignal.hpp>
#include <IModule.hpp>
#include <IVirtualRX.hpp>

class RRCCompute : public IModule, public IVirtualRX {
public:
  RRCCompute();
  ~RRCCompute() override;

  bool init() override;
  bool run() override;

  void setParam(const std::string &paramName, const std::any &value) override;
  bool setData(std::shared_ptr<IData> data) override;
  std::shared_ptr<IData> getData() override;

private:
  double m_sampleRate = 0.0;
  double m_symbolRate = 0.0;
  double m_rolloff = 0.35;
  int m_spanSymbols = 0;
  int m_samplesPerSymbol = 0;
  bool m_samplesPerSymbolExplicit = false;
  bool m_normalizeGain = true;

  std::shared_ptr<GpuFloatSignal> m_data;
  bool m_isComputed = false;
};

#endif // RRC_COMPUTE_HPP
