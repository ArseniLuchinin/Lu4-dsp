#include <PhaseRotator.hpp>

#include <GpuComplexSignal.hpp>
#include <GpuFloatSignal.hpp>
#include <VirtualTransmitter.hpp>

#include <cuda_runtime.h>
#include <gtest/gtest.h>

#include <cmath>
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

TEST(PhaseRotatorModuleTest, CompensatesInputPhase) {
  if (!isCudaAvailable()) {
    GTEST_SKIP() << "CUDA device is unavailable in test environment.";
  }

  VirtualTransmitter tx;
  auto phase = std::make_shared<GpuFloatSignal>(1);
  float phaseRaw = 0.3f;
  phase->setDataFromHost(&phaseRaw, 1);
  tx.txData(phase, "phase_test_tag_2");

  PhaseRotator module;
  module.setParam("phase tag", std::string("phase_test_tag_2"));
  ASSERT_TRUE(module.init());

  const float inPhase = 0.3f;
  ASSERT_TRUE(
      module.setData(makeGpuComplex({{std::cos(inPhase), std::sin(inPhase)}})));
  ASSERT_TRUE(module.run());

  auto out = std::dynamic_pointer_cast<GpuComplexFloatSignal>(module.getData());
  ASSERT_NE(out, nullptr);
  ASSERT_EQ(out->size(), size_t(1));

  const auto hostOut = downloadGpuComplex(out);
  ASSERT_EQ(hostOut.size(), size_t(1));
  EXPECT_NEAR(hostOut[0].x, 1.0f, 1.0e-5f);
  EXPECT_NEAR(hostOut[0].y, 0.0f, 1.0e-5f);
}

TEST(PhaseRotatorModuleTest, MissingPhaseDataReturnsFalseCurrentBehavior) {
  if (!isCudaAvailable()) {
    GTEST_SKIP() << "CUDA device is unavailable in test environment.";
  }

  const std::string missingTag = "phase_test_tag_missing_data";
  VirtualTransmitter tx;
  EXPECT_FALSE(tx.checkData(missingTag));

  PhaseRotator module;
  module.setParam("phase tag", missingTag);
  ASSERT_TRUE(module.init());

  ASSERT_TRUE(module.setData(makeGpuComplex({{1.0f, 0.0f}, {0.0f, 1.0f}})));
  auto invalidPhase = std::make_shared<GpuFloatSignal>();
  tx.txData(invalidPhase, missingTag);
  EXPECT_TRUE(tx.checkData(missingTag));

  EXPECT_FALSE(module.run());
  EXPECT_FALSE(tx.checkData(missingTag));
}
