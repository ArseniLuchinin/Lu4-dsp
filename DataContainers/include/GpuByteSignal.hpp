#ifndef GPU_BYTE_SIGNAL_HPP
#define GPU_BYTE_SIGNAL_HPP

#include <GpuSignal.hpp>

struct gpu_byte_tag {
    static constexpr const char* name = "Gpu byte signal";
};

extern template class GpuSignal<uint8_t, gpu_byte_tag>;
using GpuByteSignal = GpuSignal<uint8_t, gpu_byte_tag>;

#endif // GPU_BYTE_SIGNAL_HPP
