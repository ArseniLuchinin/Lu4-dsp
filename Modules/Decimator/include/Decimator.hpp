#ifndef DECIMATOR_HPP
#define DECIMATOR_HPP

#include <IModule.hpp>

#include <CpuComplexSignal.hpp>
#include <GpuComplexSignal.hpp>

#include <memory>

class Decimator : public IModule {
public:
    Decimator();
    ~Decimator() override;

    bool init() override;
    bool run() override;

    void setParam(const std::string& paramName, const std::any& value) override;
    bool setData(std::shared_ptr<IData> data) override;
    std::shared_ptr<IData> getData() override;

private:
    int m_samplesPerSymbol = 0;
    int m_offset = 0;
    size_t m_currentPhase = 0;
    bool m_initializedState = false;

    std::shared_ptr<GpuComplexFloatSignal> m_inData;
    std::shared_ptr<CpuComplexSignal> m_outData;
};

#endif // DECIMATOR_HPP
