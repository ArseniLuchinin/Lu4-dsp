#include <TextFileWriter.hpp>

#include <GpuByteSignal.hpp>

#include <cuda_runtime.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <unistd.h>
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

} // namespace

TEST(TextFileWriterModuleTest, WritesBytesAsIs) {
  if (!isCudaAvailable()) {
    GTEST_SKIP() << "CUDA device is unavailable in test environment.";
  }

  const std::string path =
      "/tmp/qpsk_writer_test_" + std::to_string(::getpid()) + ".txt";

  TextFileWriter writer;
  writer.setParam("file name", path);
  ASSERT_TRUE(writer.init());

  const std::vector<uint8_t> payload = {'T', 'e', 's', 't', '\n'};
  ASSERT_TRUE(writer.setData(makeGpuBytes(payload)));
  ASSERT_TRUE(writer.run());

  std::ifstream in(path, std::ios::binary);
  ASSERT_TRUE(in.is_open());

  std::vector<uint8_t> read((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
  EXPECT_EQ(read, payload);

  std::filesystem::remove(path);
}
