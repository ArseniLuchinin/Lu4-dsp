#include "FFT_cufft.hpp"

#include <module.hpp>
#include <utility>

IModule* createModule() {
    return new FFT();
}

FFT::FFT() : IModule({"FFT", "FFT-module.so", "FFT.json"}) {}

bool FFT::init() {
    return true;
}

bool FFT::run() {
    return true;
}

void FFT::setParam(const std::string& paramName, const std::any& value) {
    (void)paramName;
    (void)value;
}

bool FFT::setData(std::shared_ptr<IData> data) {
    auto gpuData = std::dynamic_pointer_cast<GpuFloatSignal>(data);
    m_data = gpuData;
    return m_data != nullptr;
}

std::shared_ptr<IData> FFT::getData() {
    return m_data;
}
