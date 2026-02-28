#ifndef GPU_FLOAT_SIGNAL_HPP
#define GPU_FLOAT_SIGNAL_HPP

#include <GpuSignal.hpp>

struct gpu_float_tag {
    static constexpr const char* name = "Gpu float signal";
};

extern template class GpuSignal<float, gpu_float_tag>;
using GpuFloatSignal = GpuSignal<float, gpu_float_tag>;

#endif