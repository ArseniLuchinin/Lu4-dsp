#ifndef CARRIER_RECOVERY_HPP
#define CARRIER_RECOVERY_HPP

#include <IModule.hpp>

#include <GpuComplexSignal.hpp>

#include <cstddef>
#include <memory>

class CarrierRecovery : public IModule {
public:
    CarrierRecovery();
    ~CarrierRecovery() override;

    bool init() override;
    bool run() override;

    void setParam(const std::string& paramName, const std::any& value) override;
    bool setData(std::shared_ptr<IData> data) override;
    std::shared_ptr<IData> getData() override;

private:
    int m_order = 4;

    std::shared_ptr<GpuComplexFloatSignal> m_inData;
    std::shared_ptr<GpuComplexFloatSignal> m_outData;

    double2* m_devSum = nullptr;
    void* m_reduceTempStorage = nullptr;
    size_t m_reduceTempStorageBytes = 0;
};

#endif // CARRIER_RECOVERY_HPP
