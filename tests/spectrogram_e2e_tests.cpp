#include <CS2AS.hpp>
#include <FFT_Shift.hpp>
#include <FFT_cufft.hpp>
#include <SpectrogramPlot.hpp>

#include <GpuComplexSignal.hpp>
#include <GpuFloatSignal.hpp>
#include <cuda_runtime.h>

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace {

bool isCudaAvailable() {
  int deviceCount = 0;
  return cudaGetDeviceCount(&deviceCount) == cudaSuccess && deviceCount > 0;
}

} // namespace

TEST(SpectrogramE2E, MiniE2E_GeneratesPngFile) {
  if (!isCudaAvailable()) {
    GTEST_SKIP() << "CUDA device is unavailable in test environment.";
  }

  constexpr size_t kFftSize = 64;
  constexpr size_t kHopSize = 32;
  constexpr size_t kFrames = 4;
  constexpr size_t kInputSize = kFftSize + (kFrames - 1) * kHopSize;
  constexpr float kSampleRate = 256.0f;

  // Generate a simple complex tone at fs/8
  std::vector<cuComplex> hostSignal(kInputSize);
  const float freq = kSampleRate / 8.0f;
  for (size_t i = 0; i < kInputSize; ++i) {
    const float phase = 2.0f * static_cast<float>(M_PI) * freq *
                        static_cast<float>(i) / kSampleRate;
    hostSignal[i] = make_cuComplex(std::cos(phase), std::sin(phase));
  }

  auto gpuSignal = std::make_shared<GpuComplexFloatSignal>(kInputSize);
  gpuSignal->setDataFromHost(hostSignal.data(), kInputSize);
  ASSERT_TRUE(gpuSignal->isValid());

  FFT fft;
  fft.setParam("fft size", static_cast<int64_t>(kFftSize));
  fft.setParam("hop size", static_cast<int64_t>(kHopSize));
  ASSERT_TRUE(fft.init());

  CS2AS cs2as;
  cs2as.setParam("fft size", static_cast<int64_t>(kFftSize));
  cs2as.setParam("normalize by fft size", true);
  ASSERT_TRUE(cs2as.init());

  FFT_Shift fftShift;
  fftShift.setParam("fft size", static_cast<int64_t>(kFftSize));
  ASSERT_TRUE(fftShift.init());

  const std::filesystem::path outPath =
      std::filesystem::temp_directory_path() / "spectrogram_e2e_test";

  SpectrogramPlot plot;
  plot.setParam("sample rate", static_cast<int64_t>(kSampleRate));
  plot.setParam("fft size", static_cast<int64_t>(kFftSize));
  plot.setParam("hop size", static_cast<int64_t>(kHopSize));
  plot.setParam("centered spectrum", true);
  plot.setParam("db min", -80.0);
  plot.setParam("db max", 0.0);
  plot.setParam("show", false);
  plot.setParam("save path", outPath.string());
  plot.setParam("png compression", int64_t(1));
  ASSERT_TRUE(plot.init());

  ASSERT_TRUE(fft.setData(gpuSignal));
  ASSERT_TRUE(fft.run());
  auto fftData = fft.getData();
  ASSERT_NE(fftData, nullptr);

  ASSERT_TRUE(cs2as.setData(fftData));
  ASSERT_TRUE(cs2as.run());
  auto ampData = cs2as.getData();
  ASSERT_NE(ampData, nullptr);

  ASSERT_TRUE(fftShift.setData(ampData));
  ASSERT_TRUE(fftShift.run());
  auto shiftedData = fftShift.getData();
  ASSERT_NE(shiftedData, nullptr);

  ASSERT_TRUE(plot.setData(shiftedData));
  ASSERT_TRUE(plot.run());

  const std::filesystem::path expectedPng = outPath.string() + "0.png";
  ASSERT_TRUE(std::filesystem::exists(expectedPng));
  EXPECT_GT(std::filesystem::file_size(expectedPng), 0u);

  std::filesystem::remove(expectedPng);
}
