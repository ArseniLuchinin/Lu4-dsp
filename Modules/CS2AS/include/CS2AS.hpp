#ifndef CS_TO_AS_HPP
#define CS_TO_AS_HPP

#include <GpuComplexSignal.hpp>
#include <GpuFloatSignal.hpp>
#include <IModule.hpp>

#include <cstddef>

class CS2AS : public IModule {
public:
  inline CS2AS() : IModule({"CS2AS", "libCS2AS-module.so", "module.json"}) {};
  ~CS2AS() = default;

  bool init() override;
  bool run() override;

  void setParam(const std::string &paramName, const std::any &value) override;

  bool setData(std::shared_ptr<IData> data) override;
  std::shared_ptr<IData> getData() override;

private:
  size_t m_fftSize = 1;
  bool m_normalizeByFftSize = true;
  std::shared_ptr<GpuComplexFloatSignal> m_inData;
  std::shared_ptr<GpuFloatSignal> m_outData;
};

#endif // CS_TO_AS_HPP
