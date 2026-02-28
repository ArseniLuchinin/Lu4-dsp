
#ifndef GPU_COMPLEX_FLOAT_SIGNAL_HPP
#define GPU_COMPLEX_FLOAT_SIGNAL_HPP

#include <IData.hpp>
#include <cuComplex.h>

class GpuComplexFloatSignal : public IData {
public:
    GpuComplexFloatSignal(const GpuComplexFloatSignal&) = delete;
    GpuComplexFloatSignal(const cuComplex* data, size_t size);
    GpuComplexFloatSignal(GpuComplexFloatSignal && other);
    //TODO GpuComplexFloatSignal(const float* realData, const float* imagData, size_t size);
    ~GpuComplexFloatSignal();

    void setDataFromHost(cuComplex* data, size_t size);
    void setDataFromDevice(cuComplex* data, const size_t size);

    cuComplex* getDeviceData();

    size_t size() const override;


private:
    bool checkData(const cuComplex* data);
    void freeData();

    cuComplex* m_data;
    size_t m_size = 0;
};

#endif

