#ifndef CARRIER_PHASE_ESTIMATOR_HPP
#define CARRIER_PHASE_ESTIMATOR_HPP

#include <IModule.hpp>

#include <GpuComplexSignal.hpp>

#include <cstddef>
#include <memory>
#include <string>

class CarrierPhaseEstimator : public IModule {
public:
    CarrierPhaseEstimator();
    ~CarrierPhaseEstimator() override;

    bool init() override;
    bool run() override;

    void setParam(const std::string& paramName, const std::any& value) override;
    bool setData(std::shared_ptr<IData> data) override;
    std::shared_ptr<IData> getData() override;

private:
    std::string m_phaseTag = "qpsk_phase";
    float m_estimatedPhase = 0.0f;

    std::shared_ptr<GpuComplexFloatSignal> m_inData;
    std::shared_ptr<IData> m_outData;

    void* m_reduceTempStorage = nullptr;
    size_t m_reduceTempStorageBytes = 0;
    void* m_reduceOut = nullptr;
};

#endif // CARRIER_PHASE_ESTIMATOR_HPP
