#include <QPSKDecision.hpp>

#include <GpuByteSignal.hpp>
#include <GpuComplexSignal.hpp>

#include <cuda_runtime.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace {

bool isCudaAvailable()
{
    int deviceCount = 0;
    return cudaGetDeviceCount(&deviceCount) == cudaSuccess && deviceCount > 0;
}

std::shared_ptr<GpuComplexFloatSignal> makeGpuComplex(const std::vector<std::pair<float, float>>& values)
{
    std::vector<cuComplex> host(values.size());
    for (size_t i = 0; i < values.size(); ++i) {
        host[i] = make_cuComplex(values[i].first, values[i].second);
    }

    auto gpu = std::make_shared<GpuComplexFloatSignal>(host.size());
    gpu->setDataFromHost(host.data(), host.size());
    return gpu;
}

std::vector<uint8_t> downloadGpuBytes(const std::shared_ptr<GpuByteSignal>& data)
{
    if (!data || !data->isValid() || !data->getDeviceData()) {
        return {};
    }

    std::vector<uint8_t> out(data->size());
    if (!out.empty()) {
        const auto err = cudaMemcpy(out.data(), data->getDeviceData(), out.size() * sizeof(uint8_t), cudaMemcpyDeviceToHost);
        if (err != cudaSuccess) {
            return {};
        }
    }
    return out;
}

} // namespace

TEST(QpskDecisionModuleTest, UsesExpectedGrayQuadrants)
{
    if (!isCudaAvailable()) {
        GTEST_SKIP() << "CUDA device is unavailable in test environment.";
    }

    QPSKDecision module;
    ASSERT_TRUE(module.init());

    ASSERT_TRUE(module.setData(makeGpuComplex({
        {0.8f, 0.9f},
        {-0.8f, 0.9f},
        {-0.8f, -0.9f},
        {0.8f, -0.9f}
    })));
    ASSERT_TRUE(module.run());

    auto bits = std::dynamic_pointer_cast<GpuByteSignal>(module.getData());
    ASSERT_NE(bits, nullptr);

    const std::vector<uint8_t> expected = {0, 0, 1, 0, 1, 1, 0, 1};
    EXPECT_EQ(downloadGpuBytes(bits), expected);
}

TEST(QpskDecisionModuleTest, BoundaryZeroFallsToNegativeSide)
{
    if (!isCudaAvailable()) {
        GTEST_SKIP() << "CUDA device is unavailable in test environment.";
    }

    QPSKDecision module;
    ASSERT_TRUE(module.init());

    ASSERT_TRUE(module.setData(makeGpuComplex({
        {0.0f, 0.8f},
        {0.8f, 0.0f},
        {0.0f, 0.0f}
    })));
    ASSERT_TRUE(module.run());

    auto bits = std::dynamic_pointer_cast<GpuByteSignal>(module.getData());
    ASSERT_NE(bits, nullptr);
    const std::vector<uint8_t> expected = {
        1, 0,
        0, 1,
        1, 1
    };
    EXPECT_EQ(downloadGpuBytes(bits), expected);
}
