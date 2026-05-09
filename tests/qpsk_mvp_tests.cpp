#include <BitPacker.hpp>
#include <CarrierRecovery.hpp>
#include <Decimator.hpp>
#include <FIR_filter.hpp>
#include <FileSrc.hpp>
#include <QPSKDecision.hpp>
#include <RRCCompute.hpp>

#include <EmptyContainer.hpp>
#include <GpuByteSignal.hpp>
#include <GpuComplexSignal.hpp>
#include <GpuFloatSignal.hpp>
#include <VirtualTransmitter.hpp>
#include <cuda_runtime.h>

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int kQpskSps = 128;
constexpr int kQpskSymbolRate = 16384;
constexpr int kSampleRate = 2097152;

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

std::vector<uint8_t> bytesToMsbBits(const std::vector<uint8_t> &bytes) {
  std::vector<uint8_t> bits;
  bits.reserve(bytes.size() * 8);

  for (const uint8_t byte : bytes) {
    for (int bit = 7; bit >= 0; --bit) {
      bits.push_back(static_cast<uint8_t>((byte >> bit) & 0x01u));
    }
  }

  return bits;
}

std::shared_ptr<GpuComplexFloatSignal>
makePhaseShiftedQpskSymbols(const std::vector<uint8_t> &bytes,
                            const float phase) {
  const std::vector<uint8_t> bits = bytesToMsbBits(bytes);
  const float invSqrt2 = 1.0f / std::sqrt(2.0f);
  const float c = std::cos(phase);
  const float s = std::sin(phase);

  const size_t symbolCount = bits.size() / 2;
  std::vector<std::pair<float, float>> hostPairs;
  hostPairs.reserve(symbolCount);

  for (size_t i = 0; i < symbolCount; ++i) {
    const uint8_t bit0 = bits[(2 * i) + 0];
    const uint8_t bit1 = bits[(2 * i) + 1];

    const float iComp = (bit0 == 0u ? 1.0f : -1.0f) * invSqrt2;
    const float qComp = (bit1 == 0u ? 1.0f : -1.0f) * invSqrt2;
    hostPairs.emplace_back((iComp * c) - (qComp * s),
                           (iComp * s) + (qComp * c));
  }

  return makeGpuComplex(hostPairs);
}

std::vector<std::pair<float, float>>
oversampleWithOffset(const std::shared_ptr<GpuComplexFloatSignal> &symbols,
                     const int samplesPerSymbol, const int offset) {
  const size_t symbolCount = symbols ? symbols->size() : 0;
  const size_t outSize = static_cast<size_t>(offset) +
                         (symbolCount * static_cast<size_t>(samplesPerSymbol));
  std::vector<std::pair<float, float>> out(outSize, {0.0f, 0.0f});

  if (!symbols || !symbols->getDeviceData()) {
    return out;
  }

  const auto host = downloadGpuComplex(symbols);
  if (host.size() != symbolCount) {
    return out;
  }

  for (size_t i = 0; i < symbolCount; ++i) {
    const size_t idx = static_cast<size_t>(offset) +
                       (i * static_cast<size_t>(samplesPerSymbol));
    out[idx] = {host[i].x, host[i].y};
  }

  return out;
}

std::vector<uint8_t> readAllBytes(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    return {};
  }
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(in),
                              std::istreambuf_iterator<char>());
}

} // namespace

TEST(QpskMvpTest, MiniE2E_QpskChain_InputEqualsOutput_Red) {
  if (!isCudaAvailable()) {
    GTEST_SKIP() << "CUDA device is unavailable in test environment.";
  }

  const std::vector<uint8_t> inputBytes = {'Q', 'P', 'S', 'K', ' ', 'm', 'i',
                                           'n', 'i', ' ', 'e', '2', 'e', '\n'};

  CarrierRecovery recovery;
  recovery.setParam("order", int64_t(4));
  ASSERT_TRUE(recovery.init());

  QPSKDecision decision;
  ASSERT_TRUE(decision.init());

  BitPacker packer;
  packer.setParam("bit order", std::string("msb-first"));
  packer.setParam("flush tail", false);
  ASSERT_TRUE(packer.init());

  ASSERT_TRUE(recovery.setData(makePhaseShiftedQpskSymbols(inputBytes, 0.3f)));
  ASSERT_TRUE(recovery.run());

  ASSERT_TRUE(decision.setData(recovery.getData()));
  ASSERT_TRUE(decision.run());

  ASSERT_TRUE(packer.setData(decision.getData()));
  ASSERT_TRUE(packer.run());

  auto decoded = std::dynamic_pointer_cast<GpuByteSignal>(packer.getData());
  ASSERT_NE(decoded, nullptr);
  EXPECT_EQ(downloadGpuBytes(decoded), inputBytes);
}

TEST(QpskMvpTest, DemodMiniE2E_WithDecimatorAndKnownOffset) {
  if (!isCudaAvailable()) {
    GTEST_SKIP() << "CUDA device is unavailable in test environment.";
  }

  const int sps = 8;
  const int offset = 3;
  const float inputPhase = 0.27f;
  const std::vector<uint8_t> inputBytes = {'D', 'e', 'c', 'i', 'm', '\n'};

  const auto symbols = makePhaseShiftedQpskSymbols(inputBytes, inputPhase);
  const auto oversampled = oversampleWithOffset(symbols, sps, offset);

  Decimator decimator;
  decimator.setParam("samples per symbol", int64_t(sps));
  decimator.setParam("offset", int64_t(offset));
  ASSERT_TRUE(decimator.init());

  CarrierRecovery recovery;
  recovery.setParam("order", int64_t(4));
  ASSERT_TRUE(recovery.init());

  QPSKDecision decision;
  ASSERT_TRUE(decision.init());

  BitPacker packer;
  packer.setParam("bit order", std::string("msb-first"));
  packer.setParam("flush tail", false);
  ASSERT_TRUE(packer.init());

  ASSERT_TRUE(decimator.setData(makeGpuComplex(oversampled)));
  ASSERT_TRUE(decimator.run());

  ASSERT_TRUE(recovery.setData(decimator.getData()));
  ASSERT_TRUE(recovery.run());

  ASSERT_TRUE(decision.setData(recovery.getData()));
  ASSERT_TRUE(decision.run());

  ASSERT_TRUE(packer.setData(decision.getData()));
  ASSERT_TRUE(packer.run());

  auto decoded = std::dynamic_pointer_cast<GpuByteSignal>(packer.getData());
  ASSERT_NE(decoded, nullptr);
  EXPECT_EQ(downloadGpuBytes(decoded), inputBytes);
}

TEST(QpskMvpTest, FileE2E_QpskRrc128_InputEqualsOutput) {
  if (!isCudaAvailable()) {
    GTEST_SKIP() << "CUDA device is unavailable in test environment.";
  }

  const std::filesystem::path repoRoot(PROJECT_SOURCE_DIR);
  const std::filesystem::path testDataRoot = repoRoot / "tests" / "TestData";
  const std::filesystem::path messagePath =
      testDataRoot / "signals" / "qpsk_rrc_message.txt";
  const std::filesystem::path signalPath =
      testDataRoot / "signals" / "qpsk_rrc_signal.bin";

  ASSERT_TRUE(std::filesystem::exists(messagePath));
  ASSERT_TRUE(std::filesystem::exists(signalPath));

  const auto messageBytes = readAllBytes(messagePath);
  ASSERT_FALSE(messageBytes.empty());

  RRCCompute rrc;
  rrc.setParam("sample rate", int64_t(kSampleRate));
  rrc.setParam("symbol rate", int64_t(kQpskSymbolRate));
  rrc.setParam("rolloff", 0.35f);
  rrc.setParam("span symbols", int64_t(8));
  rrc.setParam("samples per symbol", int64_t(kQpskSps));
  rrc.setParam("normalize gain", true);
  ASSERT_TRUE(rrc.init());
  ASSERT_TRUE(rrc.run());

  VirtualTransmitter tx;
  auto rrcTaps = rrc.getData();
  ASSERT_NE(rrcTaps, nullptr);
  tx.txData(rrcTaps, "fir_rrc_coeff_file_e2e_rrc");

  FileSrc fileSrc;
  fileSrc.setParam("file name", signalPath.string());
  fileSrc.setParam("data type", std::string("complex"));
  fileSrc.setParam("max size", int64_t(1024 * 1024 * 1024));
  ASSERT_TRUE(fileSrc.init());

  FIRFilter fir;
  fir.fetchParam("taps", std::string("@fir_rrc_coeff_file_e2e_rrc"));
  fir.setParam("filter order", int64_t((8 * kQpskSps) + 1));
  fir.setParam("coefficients type", std::string("real"));
  fir.setParam("log energy", false);
  ASSERT_TRUE(fir.init());

  Decimator decimator;
  decimator.setParam("samples per symbol", int64_t(kQpskSps));
  decimator.setParam("offset", int64_t(0));
  ASSERT_TRUE(decimator.init());

  CarrierRecovery recovery;
  recovery.setParam("order", int64_t(4));
  ASSERT_TRUE(recovery.init());

  QPSKDecision decision;
  ASSERT_TRUE(decision.init());

  BitPacker packer;
  packer.setParam("bit order", std::string("msb-first"));
  packer.setParam("flush tail", false);
  packer.setParam("discard leading bits", int64_t(16));
  ASSERT_TRUE(packer.init());

  std::vector<uint8_t> decodedBytes;

  while (true) {
    ASSERT_TRUE(fileSrc.run());
    auto srcData = fileSrc.getData();
    ASSERT_NE(srcData, nullptr);

    if (std::dynamic_pointer_cast<EmptyContainer>(srcData)) {
      break;
    }

    ASSERT_TRUE(fir.setData(srcData));
    ASSERT_TRUE(fir.run());

    ASSERT_TRUE(decimator.setData(fir.getData()));
    ASSERT_TRUE(decimator.run());

    ASSERT_TRUE(recovery.setData(decimator.getData()));
    ASSERT_TRUE(recovery.run());

    ASSERT_TRUE(decision.setData(recovery.getData()));
    ASSERT_TRUE(decision.run());

    ASSERT_TRUE(packer.setData(decision.getData()));
    ASSERT_TRUE(packer.run());

    auto chunkBytes =
        std::dynamic_pointer_cast<GpuByteSignal>(packer.getData());
    ASSERT_NE(chunkBytes, nullptr);
    const auto bytes = downloadGpuBytes(chunkBytes);
    decodedBytes.insert(decodedBytes.end(), bytes.begin(), bytes.end());
  }

  EXPECT_EQ(decodedBytes, messageBytes);
}
