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
    if (!m_data || m_data->size() == 0) {
        return false;
    }

    matplot::vector_1d y(m_data->getData(), m_data->getData() + m_data->size());
    std::vector<float> time(m_data->size());

    for (int i = 0; i < m_data->size(); ++i) {
        time[i] = float(i) / m_samplateRate;
    }

    auto f = matplot::figure();
    f->backend()->run_command("unset warnings");

    matplot::plot(time, y);

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
    auto gpuData = std::dynamic_pointer_cast<GpuFloatSignal>(data);
    m_data = CpuFloatSignal::fromGpu(gpuData);

    return static_cast<bool>(m_data);
}

std::shared_ptr<IData> SignalPlot::getData() {
    return m_data;
}