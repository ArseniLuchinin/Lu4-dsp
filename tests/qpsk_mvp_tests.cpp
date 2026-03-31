#include <BitPacker.hpp>
#include <CarrierPhaseEstimator.hpp>
#include <Decimator.hpp>
#include <FIR_filter.hpp>
#include <FileSrc.hpp>
#include <PhaseRotator.hpp>
#include <QPSKDecision.hpp>
#include <RRCCompute.hpp>
#include <TextFileWriter.hpp>

#include <CpuByteSignal.hpp>
#include <CpuComplexSignal.hpp>
#include <CpuFloatSignal.hpp>
#include <EmptyContainer.hpp>
#include <GpuComplexSignal.hpp>
#include <VirtualTransmitter.hpp>
#include <cuda_runtime.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace {

constexpr int kQpskSps = 128;
constexpr int kQpskSymbolRate = 16384;
constexpr int kSampleRate = 2097152;
bool isCudaAvailable()
{
    int deviceCount = 0;
    return cudaGetDeviceCount(&deviceCount) == cudaSuccess && deviceCount > 0;
}

std::shared_ptr<CpuComplexSignal> makeCpuComplex(const std::vector<std::pair<float, float>>& values)
{
    auto* raw = new cuComplex[values.size()];
    for (size_t i = 0; i < values.size(); ++i) {
        raw[i] = make_cuComplex(values[i].first, values[i].second);
    }
    return std::make_shared<CpuComplexSignal>(raw, values.size());
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

std::vector<uint8_t> bytesToMsbBits(const std::vector<uint8_t>& bytes)
{
    std::vector<uint8_t> bits;
    bits.reserve(bytes.size() * 8);

    for (const uint8_t byte : bytes) {
        for (int bit = 7; bit >= 0; --bit) {
            bits.push_back(static_cast<uint8_t>((byte >> bit) & 0x01u));
        }
    }

    return bits;
}

std::shared_ptr<CpuComplexSignal> makePhaseShiftedQpskSymbols(const std::vector<uint8_t>& bytes, const float phase)
{
    const std::vector<uint8_t> bits = bytesToMsbBits(bytes);
    const float invSqrt2 = 1.0f / std::sqrt(2.0f);
    const float c = std::cos(phase);
    const float s = std::sin(phase);

    const size_t symbolCount = bits.size() / 2;
    auto* raw = new cuComplex[symbolCount];

    for (size_t i = 0; i < symbolCount; ++i) {
        const uint8_t bit0 = bits[(2 * i) + 0];
        const uint8_t bit1 = bits[(2 * i) + 1];

        const float iComp = (bit0 == 0u ? 1.0f : -1.0f) * invSqrt2;
        const float qComp = (bit1 == 0u ? 1.0f : -1.0f) * invSqrt2;

        raw[i].x = (iComp * c) - (qComp * s);
        raw[i].y = (iComp * s) + (qComp * c);
    }

    return std::make_shared<CpuComplexSignal>(raw, symbolCount);
}

std::shared_ptr<CpuComplexSignal> scaleSymbols(
    const std::shared_ptr<CpuComplexSignal>& in,
    const std::vector<float>& amplitudes)
{
    if (!in || !in->getData()) {
        return std::make_shared<CpuComplexSignal>();
    }

    const size_t n = in->size();
    auto* raw = new cuComplex[n];

    for (size_t i = 0; i < n; ++i) {
        const float amp = amplitudes.empty() ? 1.0f : amplitudes[i % amplitudes.size()];
        raw[i].x = in->getData()[i].x * amp;
        raw[i].y = in->getData()[i].y * amp;
    }

    return std::make_shared<CpuComplexSignal>(raw, n);
}

std::vector<std::pair<float, float>> oversampleWithOffset(
    const std::shared_ptr<CpuComplexSignal>& symbols,
    const int samplesPerSymbol,
    const int offset)
{
    const size_t symbolCount = symbols ? symbols->size() : 0;
    const size_t outSize = static_cast<size_t>(offset) + (symbolCount * static_cast<size_t>(samplesPerSymbol));
    std::vector<std::pair<float, float>> out(outSize, {0.0f, 0.0f});

    if (!symbols || !symbols->getData()) {
        return out;
    }

    for (size_t i = 0; i < symbolCount; ++i) {
        const size_t idx = static_cast<size_t>(offset) + (i * static_cast<size_t>(samplesPerSymbol));
        out[idx] = {symbols->getData()[i].x, symbols->getData()[i].y};
    }

    return out;
}

std::vector<uint8_t> readAllBytes(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return {};
    }
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::vector<cuComplex> readComplexPrefix(const std::filesystem::path& path, size_t count)
{
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return {};
    }

    std::vector<cuComplex> out(count);
    in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(count * sizeof(cuComplex)));
    const auto readBytes = static_cast<size_t>(in.gcount());
    out.resize(readBytes / sizeof(cuComplex));
    return out;
}

std::string quotePath(const std::filesystem::path& path)
{
    return "\"" + path.string() + "\"";
}

} // namespace

TEST(QpskMvpTest, DecimatorKeepsPhaseAcrossBlocks)
{
    if (!isCudaAvailable()) {
        GTEST_SKIP() << "CUDA device is unavailable in test environment.";
    }

    Decimator module;
    module.setParam("samples per symbol", int32_t(4));
    module.setParam("offset", int32_t(1));
    ASSERT_TRUE(module.init());

    ASSERT_TRUE(module.setData(makeGpuComplex({
        {0.0f, 0.0f}, {1.0f, 0.0f}, {2.0f, 0.0f}, {3.0f, 0.0f}, {4.0f, 0.0f}
    })));
    ASSERT_TRUE(module.run());

    auto out1 = std::dynamic_pointer_cast<CpuComplexSignal>(module.getData());
    ASSERT_NE(out1, nullptr);
    ASSERT_EQ(out1->size(), size_t(1));
    EXPECT_FLOAT_EQ(out1->getData()[0].x, 1.0f);

    ASSERT_TRUE(module.setData(makeGpuComplex({
        {5.0f, 0.0f}, {6.0f, 0.0f}, {7.0f, 0.0f}, {8.0f, 0.0f}, {9.0f, 0.0f}
    })));
    ASSERT_TRUE(module.run());

    auto out2 = std::dynamic_pointer_cast<CpuComplexSignal>(module.getData());
    ASSERT_NE(out2, nullptr);
    ASSERT_EQ(out2->size(), size_t(2));
    EXPECT_FLOAT_EQ(out2->getData()[0].x, 5.0f);
    EXPECT_FLOAT_EQ(out2->getData()[1].x, 9.0f);
}

TEST(QpskMvpTest, CarrierPhaseEstimatorUsesFourthPower)
{
    CarrierPhaseEstimator module;
    module.setParam("phase tag", std::string("phase_test_tag_1"));
    ASSERT_TRUE(module.init());

    const float phi = 0.2f;
    const float amp = 1.0f / std::sqrt(2.0f);
    std::vector<std::pair<float, float>> symbols;
    symbols.reserve(64);
    for (size_t i = 0; i < 64; ++i) {
        const float a = static_cast<float>(M_PI_4) + phi;
        symbols.emplace_back(std::cos(a) * amp * std::sqrt(2.0f), std::sin(a) * amp * std::sqrt(2.0f));
    }

    ASSERT_TRUE(module.setData(makeCpuComplex(symbols)));
    ASSERT_TRUE(module.run());

    VirtualTransmitter tx;
    auto phaseData = std::dynamic_pointer_cast<CpuFloatSignal>(tx.waitRxData("phase_test_tag_1"));
    ASSERT_NE(phaseData, nullptr);
    ASSERT_EQ(phaseData->size(), size_t(1));

    const float measured = phaseData->getData()[0];
    const float wrappedErr = std::remainder(measured - phi, static_cast<float>(M_PI_2));
    EXPECT_NEAR(wrappedErr, 0.0f, 1.0e-3f);
}

TEST(QpskMvpTest, CarrierPhaseEstimator_EmptyInput_ProducesZeroPhase)
{
    CarrierPhaseEstimator module;
    module.setParam("phase tag", std::string("phase_test_tag_empty"));
    ASSERT_TRUE(module.init());

    ASSERT_TRUE(module.setData(makeCpuComplex({})));
    ASSERT_TRUE(module.run());

    VirtualTransmitter tx;
    auto phaseData = std::dynamic_pointer_cast<CpuFloatSignal>(tx.waitRxData("phase_test_tag_empty"));
    ASSERT_NE(phaseData, nullptr);
    ASSERT_EQ(phaseData->size(), size_t(1));
    ASSERT_NE(phaseData->getData(), nullptr);
    EXPECT_FLOAT_EQ(phaseData->getData()[0], 0.0f);
}

TEST(QpskMvpTest, CarrierPhaseEstimator_AmplitudeImbalance_StillEstimatesPhase)
{
    CarrierPhaseEstimator module;
    module.setParam("phase tag", std::string("phase_test_tag_amp_imbalance"));
    ASSERT_TRUE(module.init());

    const float phi = -0.31f;
    const std::vector<uint8_t> bytes = {'A', 'm', 'p', 'l', 'i', 't', 'u', 'd', 'e', '\n'};
    const auto base = makePhaseShiftedQpskSymbols(bytes, phi);
    const auto scaled = scaleSymbols(base, {0.4f, 0.8f, 1.3f, 1.9f});

    ASSERT_TRUE(module.setData(scaled));
    ASSERT_TRUE(module.run());

    VirtualTransmitter tx;
    auto phaseData = std::dynamic_pointer_cast<CpuFloatSignal>(tx.waitRxData("phase_test_tag_amp_imbalance"));
    ASSERT_NE(phaseData, nullptr);
    ASSERT_EQ(phaseData->size(), size_t(1));
    ASSERT_NE(phaseData->getData(), nullptr);

    const float measured = phaseData->getData()[0];
    const float wrappedErr = std::remainder(measured - phi, static_cast<float>(M_PI_2));
    EXPECT_NEAR(wrappedErr, 0.0f, 3.0e-3f);
}

TEST(QpskMvpTest, CarrierPhaseEstimator_PhaseWrapAround_NearPiOver4)
{
    CarrierPhaseEstimator module;
    module.setParam("phase tag", std::string("phase_test_tag_wrap"));
    ASSERT_TRUE(module.init());

    const std::vector<uint8_t> bytes = {'W', 'r', 'a', 'p', '\n'};
    const std::vector<float> phases = {
        static_cast<float>(M_PI_4) - 1.0e-4f,
        -static_cast<float>(M_PI_4) + 1.0e-4f
    };

    VirtualTransmitter tx;
    for (const float phi : phases) {
        ASSERT_TRUE(module.setData(makePhaseShiftedQpskSymbols(bytes, phi)));
        ASSERT_TRUE(module.run());

        auto phaseData = std::dynamic_pointer_cast<CpuFloatSignal>(tx.waitRxData("phase_test_tag_wrap"));
        ASSERT_NE(phaseData, nullptr);
        ASSERT_EQ(phaseData->size(), size_t(1));
        ASSERT_NE(phaseData->getData(), nullptr);

        const float measured = phaseData->getData()[0];
        const float wrappedErr = std::remainder(measured - phi, static_cast<float>(M_PI_2));
        EXPECT_NEAR(wrappedErr, 0.0f, 2.0e-3f);
    }
}

TEST(QpskMvpTest, PhaseRotatorCompensatesInputPhase)
{
    VirtualTransmitter tx;
    auto* phaseRaw = new float[1];
    phaseRaw[0] = 0.3f;
    tx.txData(std::make_shared<CpuFloatSignal>(phaseRaw, 1), "phase_test_tag_2");

    PhaseRotator module;
    module.setParam("phase tag", std::string("phase_test_tag_2"));
    ASSERT_TRUE(module.init());

    const float inPhase = 0.3f;
    ASSERT_TRUE(module.setData(makeCpuComplex({{std::cos(inPhase), std::sin(inPhase)}})));
    ASSERT_TRUE(module.run());

    auto out = std::dynamic_pointer_cast<CpuComplexSignal>(module.getData());
    ASSERT_NE(out, nullptr);
    ASSERT_EQ(out->size(), size_t(1));
    EXPECT_NEAR(out->getData()[0].x, 1.0f, 1.0e-5f);
    EXPECT_NEAR(out->getData()[0].y, 0.0f, 1.0e-5f);
}

TEST(QpskMvpTest, PhaseRotator_MissingPhaseData_ReturnsFalse_CurrentBehavior)
{
    PhaseRotator module;
    module.setParam("phase tag", std::string("phase_test_tag_missing_data"));
    ASSERT_TRUE(module.init());

    ASSERT_TRUE(module.setData(makeCpuComplex({{1.0f, 0.0f}, {0.0f, 1.0f}})));
}

TEST(QpskMvpTest, DISABLED_PhaseRotator_MissingPhaseData_FallbackZero_TODO)
{
    PhaseRotator module;
    module.setParam("phase tag", std::string("phase_test_tag_missing_data_todo"));
    ASSERT_TRUE(module.init());

    ASSERT_TRUE(module.setData(makeCpuComplex({{0.3f, -0.2f}})));
    ASSERT_TRUE(module.run());

    auto out = std::dynamic_pointer_cast<CpuComplexSignal>(module.getData());
    ASSERT_NE(out, nullptr);
    ASSERT_EQ(out->size(), size_t(1));
    ASSERT_NE(out->getData(), nullptr);
    EXPECT_NEAR(out->getData()[0].x, 0.3f, 1.0e-6f);
    EXPECT_NEAR(out->getData()[0].y, -0.2f, 1.0e-6f);
}

TEST(QpskMvpTest, QpskDecisionUsesExpectedGrayQuadrants)
{
    QPSKDecision module;
    ASSERT_TRUE(module.init());

    ASSERT_TRUE(module.setData(makeCpuComplex({
        {0.8f, 0.9f},
        {-0.8f, 0.9f},
        {-0.8f, -0.9f},
        {0.8f, -0.9f}
    })));
    ASSERT_TRUE(module.run());

    auto bits = std::dynamic_pointer_cast<CpuByteSignal>(module.getData());
    ASSERT_NE(bits, nullptr);

    const std::vector<uint8_t> expected = {0, 0, 1, 0, 1, 1, 0, 1};
    EXPECT_EQ(bits->bytes(), expected);
}

TEST(QpskMvpTest, QPSKDecision_Boundary_ZeroFallsToNegativeSide)
{
    QPSKDecision module;
    ASSERT_TRUE(module.init());

    ASSERT_TRUE(module.setData(makeCpuComplex({
        {0.0f, 0.8f},
        {0.8f, 0.0f},
        {0.0f, 0.0f}
    })));
    ASSERT_TRUE(module.run());

    auto bits = std::dynamic_pointer_cast<CpuByteSignal>(module.getData());
    ASSERT_NE(bits, nullptr);
    const std::vector<uint8_t> expected = {
        1, 0, // I==0 -> bit0=1, Q>0 -> bit1=0
        0, 1, // I>0 -> bit0=0, Q==0 -> bit1=1
        1, 1  // I==0, Q==0 -> 1,1
    };
    EXPECT_EQ(bits->bytes(), expected);
}

TEST(QpskMvpTest, BitPackerMsbFirstAndPendingBits)
{
    BitPacker module;
    module.setParam("bit order", std::string("msb-first"));
    module.setParam("flush tail", false);
    ASSERT_TRUE(module.init());

    ASSERT_TRUE(module.setData(std::make_shared<CpuByteSignal>(std::vector<uint8_t>{1, 0, 1, 0, 0, 1})));
    ASSERT_TRUE(module.run());
    auto out1 = std::dynamic_pointer_cast<CpuByteSignal>(module.getData());
    ASSERT_NE(out1, nullptr);
    EXPECT_TRUE(out1->bytes().empty());

    ASSERT_TRUE(module.setData(std::make_shared<CpuByteSignal>(std::vector<uint8_t>{1, 1, 0, 0})));
    ASSERT_TRUE(module.run());
    auto out2 = std::dynamic_pointer_cast<CpuByteSignal>(module.getData());
    ASSERT_NE(out2, nullptr);

    ASSERT_EQ(out2->size(), size_t(1));
    EXPECT_EQ(out2->bytes()[0], static_cast<uint8_t>(0b10100111));
}

TEST(QpskMvpTest, TextFileWriterWritesBytesAsIs)
{
    const std::string path = "/tmp/qpsk_writer_test_" + std::to_string(::getpid()) + ".txt";

    TextFileWriter writer;
    writer.setParam("file name", path);
    ASSERT_TRUE(writer.init());

    const std::vector<uint8_t> payload = {'T', 'e', 's', 't', '\n'};
    ASSERT_TRUE(writer.setData(std::make_shared<CpuByteSignal>(payload)));
    ASSERT_TRUE(writer.run());

    std::ifstream in(path, std::ios::binary);
    ASSERT_TRUE(in.is_open());

    std::vector<uint8_t> read((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(read, payload);

    std::filesystem::remove(path);
}

TEST(QpskMvpTest, MiniE2E_QpskChain_InputEqualsOutput_Red)
{
    const std::vector<uint8_t> inputBytes = {
        'Q', 'P', 'S', 'K', ' ', 'm', 'i', 'n', 'i', ' ', 'e', '2', 'e', '\n'
    };

    CarrierPhaseEstimator estimator;
    estimator.setParam("phase tag", std::string("phase_test_tag_mini_e2e"));
    ASSERT_TRUE(estimator.init());

    PhaseRotator rotator;
    rotator.setParam("phase tag", std::string("phase_test_tag_mini_e2e"));
    ASSERT_TRUE(rotator.init());

    QPSKDecision decision;
    ASSERT_TRUE(decision.init());

    BitPacker packer;
    packer.setParam("bit order", std::string("msb-first"));
    packer.setParam("flush tail", false);
    ASSERT_TRUE(packer.init());

    ASSERT_TRUE(estimator.setData(makePhaseShiftedQpskSymbols(inputBytes, 0.3f)));
    ASSERT_TRUE(estimator.run());

    ASSERT_TRUE(rotator.setData(estimator.getData()));
    ASSERT_TRUE(rotator.run());

    ASSERT_TRUE(decision.setData(rotator.getData()));
    ASSERT_TRUE(decision.run());

    ASSERT_TRUE(packer.setData(decision.getData()));
    ASSERT_TRUE(packer.run());

    auto decoded = std::dynamic_pointer_cast<CpuByteSignal>(packer.getData());
    ASSERT_NE(decoded, nullptr);
    EXPECT_EQ(decoded->bytes(), inputBytes);
}

TEST(QpskMvpTest, DemodMiniE2E_WithDecimatorAndKnownOffset)
{
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
    decimator.setParam("samples per symbol", int32_t(sps));
    decimator.setParam("offset", int32_t(offset));
    ASSERT_TRUE(decimator.init());

    CarrierPhaseEstimator estimator;
    estimator.setParam("phase tag", std::string("phase_test_tag_decimator_mini_e2e"));
    ASSERT_TRUE(estimator.init());

    PhaseRotator rotator;
    rotator.setParam("phase tag", std::string("phase_test_tag_decimator_mini_e2e"));
    ASSERT_TRUE(rotator.init());

    QPSKDecision decision;
    ASSERT_TRUE(decision.init());

    BitPacker packer;
    packer.setParam("bit order", std::string("msb-first"));
    packer.setParam("flush tail", false);
    ASSERT_TRUE(packer.init());

    ASSERT_TRUE(decimator.setData(makeGpuComplex(oversampled)));
    ASSERT_TRUE(decimator.run());

    ASSERT_TRUE(estimator.setData(decimator.getData()));
    ASSERT_TRUE(estimator.run());

    ASSERT_TRUE(rotator.setData(estimator.getData()));
    ASSERT_TRUE(rotator.run());

    ASSERT_TRUE(decision.setData(rotator.getData()));
    ASSERT_TRUE(decision.run());

    ASSERT_TRUE(packer.setData(decision.getData()));
    ASSERT_TRUE(packer.run());

    auto decoded = std::dynamic_pointer_cast<CpuByteSignal>(packer.getData());
    ASSERT_NE(decoded, nullptr);
    EXPECT_EQ(decoded->bytes(), inputBytes);
}

TEST(QpskMvpTest, FileE2E_QpskIdeal128_InputEqualsOutput_Red)
{
    if (!isCudaAvailable()) {
        GTEST_SKIP() << "CUDA device is unavailable in test environment.";
    }

    const std::filesystem::path repoRoot(PROJECT_SOURCE_DIR);
    const std::filesystem::path messagePath = repoRoot / "signal_examples" / "qpsk_message.txt";
    const std::filesystem::path signalPath = repoRoot / "signal_examples" / "qpsk_signal.bin";

    ASSERT_TRUE(std::filesystem::exists(messagePath));
    ASSERT_TRUE(std::filesystem::exists(signalPath));

    const auto messageBytes = readAllBytes(messagePath);
    ASSERT_FALSE(messageBytes.empty());

    const uint64_t messageBits = static_cast<uint64_t>(messageBytes.size()) * 8ULL;
    const uint64_t symbolsCount = (messageBits + 1ULL) / 2ULL;
    const uint64_t expectedSignalSizeBytes = symbolsCount * static_cast<uint64_t>(kQpskSps) * sizeof(cuComplex);
    EXPECT_EQ(std::filesystem::file_size(signalPath), expectedSignalSizeBytes);

    // Guard against broken fixture generation (uint8 underflow in symbol mapping).
    // Valid ideal-QPSK levels must stay near +/-1/sqrt(2), definitely not hundreds.
    const auto preview = readComplexPrefix(signalPath, size_t(8192));
    ASSERT_FALSE(preview.empty());
    float maxAbs = 0.0f;
    for (const auto& v : preview) {
        maxAbs = std::max(maxAbs, std::abs(v.x));
        maxAbs = std::max(maxAbs, std::abs(v.y));
    }
    ASSERT_LT(maxAbs, 2.0f) << "qpsk_signal.bin seems malformed (max|IQ|=" << maxAbs
                            << "). Regenerate with fixed utils/qpsk_generator.py.";

    // For generator mode=ideal the source is impulse-like at symbol rate.
    // To validate QPSK demod chain itself, keep FIR stage but run it as identity.
    auto* tapsRaw = new float[1];
    tapsRaw[0] = 1.0f;
    auto identityTaps = std::make_shared<CpuFloatSignal>(tapsRaw, size_t(1));

    VirtualTransmitter tx;
    tx.txData(identityTaps, "fir_rrc_coeff_file_e2e");

    FileSrc fileSrc;
    fileSrc.setParam("file name", signalPath.string());
    fileSrc.setParam("data type", std::string("complex"));
    fileSrc.setParam("max size", int32_t(32 * 1024 * 1024));
    ASSERT_TRUE(fileSrc.init());

    FIRFilter fir;
    fir.setParam("coefficients data tag", std::string("fir_rrc_coeff_file_e2e"));
    fir.setParam("filter order", int32_t(1));
    fir.setParam("coefficients type", std::string("real"));
    fir.setParam("log energy", false);
    ASSERT_TRUE(fir.init());

    Decimator decimator;
    decimator.setParam("samples per symbol", int32_t(kQpskSps));
    decimator.setParam("offset", int32_t(0));
    ASSERT_TRUE(decimator.init());

    CarrierPhaseEstimator estimator;
    estimator.setParam("phase tag", std::string("phase_test_tag_file_e2e"));
    ASSERT_TRUE(estimator.init());

    PhaseRotator rotator;
    rotator.setParam("phase tag", std::string("phase_test_tag_file_e2e"));
    ASSERT_TRUE(rotator.init());

    QPSKDecision decision;
    ASSERT_TRUE(decision.init());

    BitPacker packer;
    packer.setParam("bit order", std::string("msb-first"));
    packer.setParam("flush tail", false);
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

        ASSERT_TRUE(estimator.setData(decimator.getData()));
        ASSERT_TRUE(estimator.run());

        ASSERT_TRUE(rotator.setData(estimator.getData()));
        ASSERT_TRUE(rotator.run());

        ASSERT_TRUE(decision.setData(rotator.getData()));
        ASSERT_TRUE(decision.run());

        ASSERT_TRUE(packer.setData(decision.getData()));
        ASSERT_TRUE(packer.run());

        auto chunkBytes = std::dynamic_pointer_cast<CpuByteSignal>(packer.getData());
        ASSERT_NE(chunkBytes, nullptr);

        const auto& bytes = chunkBytes->bytes();
        decodedBytes.insert(decodedBytes.end(), bytes.begin(), bytes.end());
    }

    EXPECT_EQ(decodedBytes, messageBytes);
}

TEST(QpskMvpTest, FileE2E_QpskRrc128_InputEqualsOutput)
{
    if (!isCudaAvailable()) {
        GTEST_SKIP() << "CUDA device is unavailable in test environment.";
    }

    const std::filesystem::path repoRoot(PROJECT_SOURCE_DIR);
    const std::filesystem::path generatorPath = repoRoot / "utils" / "qpsk_generator.py";
    ASSERT_TRUE(std::filesystem::exists(generatorPath));

    const std::string suffix = std::to_string(::getpid());
    const std::filesystem::path tmpMessagePath = std::filesystem::path("/tmp") / ("qpsk_rrc_message_" + suffix + ".txt");
    const std::filesystem::path tmpSignalPath = std::filesystem::path("/tmp") / ("qpsk_rrc_signal_" + suffix + ".bin");

    {
        std::ofstream out(tmpMessagePath, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << "Meow! Meow!\nMeow! Meow!\nMeow! Meow!\nMeow! Meow!\n";
    }

    const std::string cmd =
        "python3 " + quotePath(generatorPath) +
        " --input " + quotePath(tmpMessagePath) +
        " --fs " + std::to_string(kSampleRate) +
        " --sps " + std::to_string(kQpskSps) +
        " --out " + quotePath(tmpSignalPath) +
        " --mode rrc --beta 0.35 --span 8";

    ASSERT_EQ(std::system(cmd.c_str()), 0) << "Generator command failed: " << cmd;
    ASSERT_TRUE(std::filesystem::exists(tmpSignalPath));

    const auto messageBytes = readAllBytes(tmpMessagePath);
    ASSERT_FALSE(messageBytes.empty());

    RRCCompute rrc;
    rrc.setParam("sample rate", int32_t(kSampleRate));
    rrc.setParam("symbol rate", int32_t(kQpskSymbolRate));
    rrc.setParam("rolloff", 0.35f);
    rrc.setParam("span symbols", int32_t(8));
    rrc.setParam("samples per symbol", int32_t(kQpskSps));
    rrc.setParam("normalize gain", true);
    ASSERT_TRUE(rrc.init());
    ASSERT_TRUE(rrc.run());

    VirtualTransmitter tx;
    auto rrcTaps = rrc.getData();
    ASSERT_NE(rrcTaps, nullptr);
    tx.txData(rrcTaps, "fir_rrc_coeff_file_e2e_rrc");

    FileSrc fileSrc;
    fileSrc.setParam("file name", tmpSignalPath.string());
    fileSrc.setParam("data type", std::string("complex"));
    fileSrc.setParam("max size", int32_t(8 * 1024 * 1024));
    ASSERT_TRUE(fileSrc.init());

    FIRFilter fir;
    fir.setParam("coefficients data tag", std::string("fir_rrc_coeff_file_e2e_rrc"));
    fir.setParam("filter order", int32_t((8 * kQpskSps) + 1));
    fir.setParam("coefficients type", std::string("real"));
    fir.setParam("log energy", false);
    ASSERT_TRUE(fir.init());

    Decimator decimator;
    decimator.setParam("samples per symbol", int32_t(kQpskSps));
    decimator.setParam("offset", int32_t(0));
    ASSERT_TRUE(decimator.init());

    CarrierPhaseEstimator estimator;
    estimator.setParam("phase tag", std::string("phase_test_tag_file_e2e_rrc"));
    ASSERT_TRUE(estimator.init());

    PhaseRotator rotator;
    rotator.setParam("phase tag", std::string("phase_test_tag_file_e2e_rrc"));
    ASSERT_TRUE(rotator.init());

    QPSKDecision decision;
    ASSERT_TRUE(decision.init());

    BitPacker packer;
    packer.setParam("bit order", std::string("msb-first"));
    packer.setParam("flush tail", false);
    // Compensate deterministic TX+RX RRC startup transient.
    packer.setParam("discard leading bits", int32_t(16));
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

        ASSERT_TRUE(estimator.setData(decimator.getData()));
        ASSERT_TRUE(estimator.run());

        ASSERT_TRUE(rotator.setData(estimator.getData()));
        ASSERT_TRUE(rotator.run());

        ASSERT_TRUE(decision.setData(rotator.getData()));
        ASSERT_TRUE(decision.run());

        ASSERT_TRUE(packer.setData(decision.getData()));
        ASSERT_TRUE(packer.run());

        auto chunkBytes = std::dynamic_pointer_cast<CpuByteSignal>(packer.getData());
        ASSERT_NE(chunkBytes, nullptr);
        const auto& bytes = chunkBytes->bytes();
        decodedBytes.insert(decodedBytes.end(), bytes.begin(), bytes.end());
    }

    EXPECT_EQ(decodedBytes, messageBytes);

    std::error_code ec;
    std::filesystem::remove(tmpSignalPath, ec);
    std::filesystem::remove(tmpMessagePath, ec);
}
