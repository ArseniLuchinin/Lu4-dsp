#ifndef QPSK_DECISION_HPP
#define QPSK_DECISION_HPP

#include <IModule.hpp>

#include <GpuByteSignal.hpp>
#include <GpuComplexSignal.hpp>

#include <memory>

class QPSKDecision : public IModule {
public:
  QPSKDecision();
  ~QPSKDecision() override;

  bool init() override;
  bool run() override;

  void setParam(const std::string &paramName, const std::any &value) override;
  bool setData(std::shared_ptr<IData> data) override;
  std::shared_ptr<IData> getData() override;

private:
  std::shared_ptr<GpuComplexFloatSignal> m_inData;
  std::shared_ptr<GpuByteSignal> m_outData;
};

#endif // QPSK_DECISION_HPP
