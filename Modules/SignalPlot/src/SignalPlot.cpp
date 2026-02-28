#include <SignalPlot.hpp>

#include <CpuFloatSignal.hpp>
#include <GpuFloatSignal.hpp>
#include <module.hpp>

#include <cuda_runtime.h>
#include <matplot/matplot.h>

#include <memory>
#include <vector>

IModule* createModule() {
    return new SignalPlot();
}

SignalPlot::SignalPlot() : IModule({"SignalPlot", "SignalPlot.so", "SignalPlot.json"}) {
};

SignalPlot::~SignalPlot() = default;

bool SignalPlot::init() {
    return true;
}

bool SignalPlot::run() {
    if (m_data.empty()) {
        return false;
    }

    std::vector<float> time(m_data.size());

    for (int i = 0; i < m_data.size(); ++i) {
        time[i] = float(i) / m_samplateRate;
    }

    auto f = matplot::figure();
    f->backend()->run_command("unset warnings");

    matplot::plot(time, m_data);

    if(m_isShow) {

        matplot::show(); 
        INFO << "Clope Plot";
    }

    if(not m_savePath.empty()) {
        matplot::save(m_savePath);
        INFO << "Saved to " << m_savePath;
    }


    return false;
}

void SignalPlot::setParam(const std::string& paramName, const std::any& value) {
    if(paramName == "sample rate") {
        m_samplateRate = std::any_cast<int32_t>(value);
        return;
    }

    if(paramName == "save path") {
        m_savePath = std::any_cast<std::string>(value);
        return;
    }

    if(paramName == "show"){
        m_isShow = std::any_cast<bool>(value);
        return;
    }

}

bool SignalPlot::setData(std::shared_ptr<IData> data) {
    if(not data) {
        return false;
    }

    m_transitData = data;
    auto gpuData = std::dynamic_pointer_cast<GpuFloatSignal>(data);
    auto cpuData = CpuFloatSignal::fromGpu(gpuData);

    m_data = matplot::vector_1d(cpuData->getData(), cpuData->getData() + cpuData->size());

    return true;
}

std::shared_ptr<IData> SignalPlot::getData() {
    return m_transitData;
}