#ifndef GPU_FLOAT_SIGNAL_HPP
#define GPU_FLOAT_SIGNAL_HPP

#include <GpuSignal.hpp>

struct gpu_float_tag {
    static constexpr const char* name = "GpuFloatSignal";
};

using GpuFloatSignal = GpuSignal<float, gpu_float_tag>;

#endif
