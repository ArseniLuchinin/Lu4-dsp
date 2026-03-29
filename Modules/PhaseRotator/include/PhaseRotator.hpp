#ifndef PHASE_ROTATOR_HPP
#define PHASE_ROTATOR_HPP

#include <IModule.hpp>
#include <IVirtualRX.hpp>

#include <CpuComplexSignal.hpp>

#include <memory>
#include <string>

class PhaseRotator : public IModule, public IVirtualRX {
public:
    PhaseRotator();
    ~PhaseRotator() override;

    bool init() override;
    bool run() override;

    void setParam(const std::string& paramName, const std::any& value) override;
    bool setData(std::shared_ptr<IData> data) override;
    std::shared_ptr<IData> getData() override;

private:
    std::string m_phaseTag = "qpsk_phase";

    std::shared_ptr<CpuComplexSignal> m_inData;
    std::shared_ptr<CpuComplexSignal> m_outData;
};

#endif // PHASE_ROTATOR_HPP
