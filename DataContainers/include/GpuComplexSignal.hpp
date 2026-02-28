
#ifndef GPU_COMPLEX_FLOAT_SIGNAL_HPP
#define GPU_COMPLEX_FLOAT_SIGNAL_HPP

#include <cuComplex.h>
#include <GpuSignal.hpp>
struct gpu_comples_float_tag {
    static constexpr const char* name = "Gpu conmplex float signal";
};

extern template class GpuSignal<cuComplex, gpu_comples_float_tag>;
using GpuComplexFloatSignal = GpuSignal<cuComplex, gpu_comples_float_tag>;


#endif

