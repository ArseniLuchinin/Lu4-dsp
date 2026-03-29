#include <QPSKDecision.hpp>

#include <module.hpp>

#include <vector>

IModule* createModule() {
    return new QPSKDecision();
}

QPSKDecision::QPSKDecision()
    : IModule({"QPSKDecision", "", ""})
{}

QPSKDecision::~QPSKDecision() = default;

bool QPSKDecision::init() {
    return true;
}

bool QPSKDecision::setData(std::shared_ptr<IData> data) {
    m_inData = std::dynamic_pointer_cast<CpuComplexSignal>(data);
    if (!m_inData) {
        ERROR << "QPSKDecision::setData failed: input must be CpuComplexSignal." << std::endl;
        return false;
    }

    if (!m_inData->isValid()) {
        ERROR << "QPSKDecision::setData failed: input signal is invalid." << std::endl;
        return false;
    }

    return true;
}

bool QPSKDecision::run() {
    if (!m_inData) {
        ERROR << "QPSKDecision::run failed: input data is null." << std::endl;
        return false;
    }

    const size_t n = m_inData->size();
    std::vector<uint8_t> bits;
    bits.reserve(n * 2);

    const cuComplex* symbols = m_inData->getData();
    for (size_t i = 0; i < n; ++i) {
        const uint8_t bit0 = (symbols[i].x > 0.0f) ? 0u : 1u;
        const uint8_t bit1 = (symbols[i].y > 0.0f) ? 0u : 1u;
        bits.push_back(bit0);
        bits.push_back(bit1);
    }

    m_outData = std::make_shared<CpuByteSignal>(std::move(bits));
    return true;
}

void QPSKDecision::setParam(const std::string& paramName, const std::any& value) {
    (void)value;
    ERROR << "QPSKDecision::setParam unknown parameter: " << paramName << std::endl;
}

std::shared_ptr<IData> QPSKDecision::getData() {
    return m_outData;
}
