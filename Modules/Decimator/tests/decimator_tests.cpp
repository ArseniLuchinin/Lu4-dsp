#include <Decimator.hpp>

#include <GpuComplexSignal.hpp>

#include <cuda_runtime.h>
#include <gtest/gtest.h>

#include <memory>
#include <utility>
#include <vector>

namespace {

bool isCudaAvailable() {
  int deviceCount = 0;
  return cudaGetDeviceCount(&deviceCount) == cudaSuccess && deviceCount > 0;
}

std::shared_ptr<GpuComplexFloatSignal>
makeGpuComplex(const std::vector<std::pair<float, float>> &values) {
  std::vector<cuComplex> host(values.size());
  for (size_t i = 0; i < values.size(); ++i) {
    host[i] = make_cuComplex(values[i].first, values[i].second);
  }

  auto gpu = std::make_shared<GpuComplexFloatSignal>(host.size());
  gpu->setDataFromHost(host.data(), host.size());
  return gpu;
}

std::vector<cuComplex>
downloadGpuComplex(const std::shared_ptr<GpuComplexFloatSignal> &data) {
  if (!data || !data->isValid() || !data->getDeviceData()) {
    return {};
  }

  std::vector<cuComplex> out(data->size());
  if (!out.empty()) {
    const auto err =
        cudaMemcpy(out.data(), data->getDeviceData(),
                   out.size() * sizeof(cuComplex), cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
      return {};
    }
  }
  return out;
}

} // namespace

TEST(DecimatorModuleTest, DecimatorKeepsPhaseAcrossBlocks) {
  if (!isCudaAvailable()) {
    GTEST_SKIP() << "CUDA device is unavailable in test environment.";
  }

  Decimator module;
  module.setParam("samples per symbol", int64_t(4));
  module.setParam("offset", int64_t(1));
  ASSERT_TRUE(module.init());

  ASSERT_TRUE(module.setData(makeGpuComplex(
      {{0.0f, 0.0f}, {1.0f, 0.0f}, {2.0f, 0.0f}, {3.0f, 0.0f}, {4.0f, 0.0f}})));
  ASSERT_TRUE(module.run());

  auto out1 =
      std::dynamic_pointer_cast<GpuComplexFloatSignal>(module.getData());
  ASSERT_NE(out1, nullptr);
  ASSERT_EQ(out1->size(), size_t(1));
  const auto out1Host = downloadGpuComplex(out1);
  ASSERT_EQ(out1Host.size(), size_t(1));
  EXPECT_FLOAT_EQ(out1Host[0].x, 1.0f);

  ASSERT_TRUE(module.setData(makeGpuComplex(
      {{5.0f, 0.0f}, {6.0f, 0.0f}, {7.0f, 0.0f}, {8.0f, 0.0f}, {9.0f, 0.0f}})));
  ASSERT_TRUE(module.run());

  auto out2 =
      std::dynamic_pointer_cast<GpuComplexFloatSignal>(module.getData());
  ASSERT_NE(out2, nullptr);
  ASSERT_EQ(out2->size(), size_t(2));
  const auto out2Host = downloadGpuComplex(out2);
  ASSERT_EQ(out2Host.size(), size_t(2));
  EXPECT_FLOAT_EQ(out2Host[0].x, 5.0f);
  EXPECT_FLOAT_EQ(out2Host[1].x, 9.0f);
}
