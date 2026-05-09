#include <BitPacker.hpp>

#include <GpuByteSignal.hpp>

#include <cuda_runtime.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

bool isCudaAvailable() {
  int deviceCount = 0;
  return cudaGetDeviceCount(&deviceCount) == cudaSuccess && deviceCount > 0;
}

std::shared_ptr<GpuByteSignal>
makeGpuBytes(const std::vector<uint8_t> &values) {
  auto gpu = std::make_shared<GpuByteSignal>(
      std::max<size_t>(size_t(1), values.size()));
  if (!gpu || !gpu->isValid()) {
    return std::make_shared<GpuByteSignal>();
  }

  if (!values.empty()) {
    auto host = values;
    gpu->setDataFromHost(host.data(), host.size());
  } else {
    gpu->setLogicalSize(0);
  }
  return gpu;
}

std::vector<uint8_t>
downloadGpuBytes(const std::shared_ptr<GpuByteSignal> &data) {
  if (!data || !data->isValid() || !data->getDeviceData()) {
    return {};
  }

  std::vector<uint8_t> out(data->size());
  if (!out.empty()) {
    const auto err =
        cudaMemcpy(out.data(), data->getDeviceData(),
                   out.size() * sizeof(uint8_t), cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
      return {};
    }
  }
  return out;
}

} // namespace

TEST(BitPackerModuleTest, MsbFirstAndPendingBits) {
  if (!isCudaAvailable()) {
    GTEST_SKIP() << "CUDA device is unavailable in test environment.";
  }

  BitPacker module;
  module.setParam("bit order", std::string("msb-first"));
  module.setParam("flush tail", false);
  ASSERT_TRUE(module.init());

  ASSERT_TRUE(module.setData(makeGpuBytes({1, 0, 1, 0, 0, 1})));
  ASSERT_TRUE(module.run());
  auto out1 = std::dynamic_pointer_cast<GpuByteSignal>(module.getData());
  ASSERT_NE(out1, nullptr);
  EXPECT_TRUE(downloadGpuBytes(out1).empty());

  ASSERT_TRUE(module.setData(makeGpuBytes({1, 1, 0, 0})));
  ASSERT_TRUE(module.run());
  auto out2 = std::dynamic_pointer_cast<GpuByteSignal>(module.getData());
  ASSERT_NE(out2, nullptr);

  ASSERT_EQ(out2->size(), size_t(1));
  const auto out2Host = downloadGpuBytes(out2);
  ASSERT_EQ(out2Host.size(), size_t(1));
  EXPECT_EQ(out2Host[0], static_cast<uint8_t>(0b10100111));
}
