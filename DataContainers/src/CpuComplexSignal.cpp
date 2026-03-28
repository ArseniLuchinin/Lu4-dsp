#include <CpuComplexSignal.hpp>

#include <iostream>

CpuComplexSignal::CpuComplexSignal(cuComplex* data, size_t size)
    : IData("CPU complex signal")
    , m_data(data)
    , m_size(size)
{}

std::shared_ptr<CpuComplexSignal> CpuComplexSignal::fromGpu(std::shared_ptr<IData> iData) {
    auto gpuData = std::dynamic_pointer_cast<GpuComplexFloatSignal>(iData);
    if (gpuData == nullptr) {
        std::cerr << "CpuComplexSignal::fromGpu: Input IData is not a GpuComplexFloatSignal." << std::endl;
        return std::make_shared<CpuComplexSignal>();
    }

    if (gpuData->size() == 0) {
        std::cerr << "CpuComplexSignal::fromGpu: GpuComplexFloatSignal has size 0." << std::endl;
        return std::make_shared<CpuComplexSignal>();
    }

    cuComplex* data = new cuComplex[gpuData->size()];
    cudaError_t err = cudaMemcpy(
        data,
        gpuData->getDeviceData(),
        gpuData->size() * sizeof(cuComplex),
        cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        std::cerr << "CpuComplexSignal::fromGpu: cudaMemcpy failed: " << cudaGetErrorString(err) << std::endl;
        delete[] data;
        return std::make_shared<CpuComplexSignal>();
    }

    return std::make_shared<CpuComplexSignal>(data, gpuData->size());
}

CpuComplexSignal::~CpuComplexSignal() {
    if (m_data) {
        delete[] m_data;
        m_data = nullptr;
        m_size = 0;
    }
}

size_t CpuComplexSignal::size() const {
    return m_size;
}

cuComplex* CpuComplexSignal::getData() const {
    return m_data;
}
