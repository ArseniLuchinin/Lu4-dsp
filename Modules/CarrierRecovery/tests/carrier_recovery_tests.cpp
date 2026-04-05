#include <CarrierRecovery.hpp>

#include <GpuComplexSignal.hpp>

#include <cuda_runtime.h>
#include <gtest/gtest.h>

#include <cmath>
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

std::vector<cuComplex> downloadGpuComplex(const std::shared_ptr<GpuComplexFloatSignal>& data)
{
    if (!data || !data->isValid() || !data->getDeviceData()) {
        return {};
    }

    std::vector<cuComplex> out(data->size());
    if (!out.empty()) {
        const auto err = cudaMemcpy(out.data(), data->getDeviceData(), out.size() * sizeof(cuComplex), cudaMemcpyDeviceToHost);
        if (err != cudaSuccess) {
            return {};
        }
    }
    return out;
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

std::shared_ptr<GpuComplexFloatSignal> makePhaseShiftedQpskSymbols(const std::vector<uint8_t>& bytes, const float phase)
{
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
        hostPairs.emplace_back((iComp * c) - (qComp * s), (iComp * s) + (qComp * c));
    }

    return makeGpuComplex(hostPairs);
}

std::shared_ptr<GpuComplexFloatSignal> makePhaseShiftedBpskSymbols(const std::vector<uint8_t>& bytes, const float phase)
{
    const std::vector<uint8_t> bits = bytesToMsbBits(bytes);
    const float c = std::cos(phase);
    const float s = std::sin(phase);

    const size_t symbolCount = bits.size();
    std::vector<std::pair<float, float>> hostPairs;
    hostPairs.reserve(symbolCount);

    for (size_t i = 0; i < symbolCount; ++i) {
        const float re = (bits[i] == 0u ? 1.0f : -1.0f);
        const float im = 0.0f;
        hostPairs.emplace_back((re * c) - (im * s), (re * s) + (im * c));
    }

    return makeGpuComplex(hostPairs);
}

float phaseOf(const cuComplex& z)
{
    return std::atan2f(z.y, z.x);
}

} // namespace

TEST(CarrierRecoveryModuleTest, EstimatesPhaseCorrectly_QPSKOrder4)
{
    if (!isCudaAvailable()) {
        GTEST_SKIP() << "CUDA device is unavailable in test environment.";
    }

    const float phi = 0.3f;
    const std::vector<uint8_t> bytes = {'T', 'e', 's', 't', '\n'};
    const auto input = makePhaseShiftedQpskSymbols(bytes, phi);

    CarrierRecovery module;
    module.setParam("order", int32_t(4));
    ASSERT_TRUE(module.init());
    ASSERT_TRUE(module.setData(input));
    ASSERT_TRUE(module.run());

    auto out = std::dynamic_pointer_cast<GpuComplexFloatSignal>(module.getData());
    ASSERT_NE(out, nullptr);
    ASSERT_EQ(out->size(), input->size());

    const auto hostOut = downloadGpuComplex(out);
    ASSERT_FALSE(hostOut.empty());

    for (const auto& z : hostOut) {
        const float residualPhase = std::remainder(phaseOf(z), static_cast<float>(M_PI_2));
        const float normalizedResidual = std::abs(residualPhase) - static_cast<float>(M_PI_4);
        EXPECT_NEAR(std::abs(normalizedResidual), 0.0f, 1.0e-3f) << "residual phase too large";
    }
}

TEST(CarrierRecoveryModuleTest, EstimatesPhaseCorrectly_BPSKOrder2)
{
    if (!isCudaAvailable()) {
        GTEST_SKIP() << "CUDA device is unavailable in test environment.";
    }

    const float phi = 0.25f;
    const std::vector<uint8_t> bytes = {'B', 'P', 'S', 'K'};
    const auto input = makePhaseShiftedBpskSymbols(bytes, phi);

    CarrierRecovery module;
    module.setParam("order", int32_t(2));
    ASSERT_TRUE(module.init());
    ASSERT_TRUE(module.setData(input));
    ASSERT_TRUE(module.run());

    auto out = std::dynamic_pointer_cast<GpuComplexFloatSignal>(module.getData());
    ASSERT_NE(out, nullptr);
    ASSERT_EQ(out->size(), input->size());

    const auto hostOut = downloadGpuComplex(out);
    ASSERT_FALSE(hostOut.empty());

    for (const auto& z : hostOut) {
        const float residualPhase = std::remainder(phaseOf(z), static_cast<float>(M_PI));
        EXPECT_NEAR(residualPhase, 0.0f, 1.0e-3f) << "residual phase too large";
    }
}

TEST(CarrierRecoveryModuleTest, EmptyInputPassesThrough)
{
    if (!isCudaAvailable()) {
        GTEST_SKIP() << "CUDA device is unavailable in test environment.";
    }

    CarrierRecovery module;
    module.setParam("order", int32_t(4));
    ASSERT_TRUE(module.init());
    ASSERT_TRUE(module.setData(makeGpuComplex({})));
    ASSERT_TRUE(module.run());

    auto out = std::dynamic_pointer_cast<GpuComplexFloatSignal>(module.getData());
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->size(), size_t(0));
}

TEST(CarrierRecoveryModuleTest, InvalidOrder_Fails)
{
    CarrierRecovery moduleZero;
    moduleZero.setParam("order", int32_t(0));
    EXPECT_FALSE(moduleZero.init());

    CarrierRecovery moduleNegative;
    moduleNegative.setParam("order", int32_t(-1));
    EXPECT_FALSE(moduleNegative.init());
}

TEST(CarrierRecoveryModuleTest, PhaseWrapNearBoundary)
{
    if (!isCudaAvailable()) {
        GTEST_SKIP() << "CUDA device is unavailable in test environment.";
    }

    const std::vector<uint8_t> bytes = {'W', 'r', 'a', 'p', '\n'};
    const float halfPi = static_cast<float>(M_PI_2);
    const float quarterPi = halfPi * 0.5f;
    const std::vector<float> phases = {
        quarterPi - 1.0e-4f,
        -quarterPi + 1.0e-4f
    };

    for (const float phi : phases) {
        CarrierRecovery module;
        module.setParam("order", int32_t(4));
        ASSERT_TRUE(module.init());
        ASSERT_TRUE(module.setData(makePhaseShiftedQpskSymbols(bytes, phi)));
        ASSERT_TRUE(module.run());

        auto out = std::dynamic_pointer_cast<GpuComplexFloatSignal>(module.getData());
        ASSERT_NE(out, nullptr);

        const auto hostOut = downloadGpuComplex(out);
        ASSERT_FALSE(hostOut.empty());

        for (const auto& z : hostOut) {
            const float residualPhase = std::remainder(phaseOf(z), halfPi);
            const float normalizedResidual = std::abs(residualPhase) - static_cast<float>(M_PI_4);
            EXPECT_NEAR(std::abs(normalizedResidual), 0.0f, 2.0e-3f) << "phase wrap failed at phi=" << phi;
        }
    }
}

TEST(CarrierRecoveryModuleTest, AmplitudeImbalanceStillRecovers)
{
    if (!isCudaAvailable()) {
        GTEST_SKIP() << "CUDA device is unavailable in test environment.";
    }

    const float phi = -0.31f;
    const std::vector<uint8_t> bytes = {'A', 'm', 'p', '\n'};
    auto base = makePhaseShiftedQpskSymbols(bytes, phi);

    auto host = downloadGpuComplex(base);
    const std::vector<float> amplitudes = {0.4f, 0.8f, 1.3f, 1.9f};
    for (size_t i = 0; i < host.size(); ++i) {
        const float amp = amplitudes[i % amplitudes.size()];
        host[i].x *= amp;
        host[i].y *= amp;
    }

    std::vector<std::pair<float, float>> pairs;
    pairs.reserve(host.size());
    for (const auto& z : host) {
        pairs.emplace_back(z.x, z.y);
    }
    const auto scaled = makeGpuComplex(pairs);

    CarrierRecovery module;
    module.setParam("order", int32_t(4));
    ASSERT_TRUE(module.init());
    ASSERT_TRUE(module.setData(scaled));
    ASSERT_TRUE(module.run());

    auto out = std::dynamic_pointer_cast<GpuComplexFloatSignal>(module.getData());
    ASSERT_NE(out, nullptr);

    const auto hostOut = downloadGpuComplex(out);
    ASSERT_FALSE(hostOut.empty());

    const float halfPi = static_cast<float>(M_PI_2);
    for (const auto& z : hostOut) {
        const float residualPhase = std::remainder(phaseOf(z), halfPi);
        const float normalizedResidual = std::abs(residualPhase) - static_cast<float>(M_PI_4);
        EXPECT_NEAR(std::abs(normalizedResidual), 0.0f, 3.0e-3f) << "amplitude imbalance recovery failed";
    }
}

TEST(CarrierRecoveryModuleTest, StatelessAcrossBlocks)
{
    if (!isCudaAvailable()) {
        GTEST_SKIP() << "CUDA device is unavailable in test environment.";
    }

    const float phi1 = 0.2f;
    const float phi2 = -0.15f;
    const std::vector<uint8_t> bytes1 = {'B', 'l', 'o', 'c', 'k', '1'};
    const std::vector<uint8_t> bytes2 = {'B', 'l', 'o', 'c', 'k', '2'};

    CarrierRecovery module;
    module.setParam("order", int32_t(4));
    ASSERT_TRUE(module.init());

    ASSERT_TRUE(module.setData(makePhaseShiftedQpskSymbols(bytes1, phi1)));
    ASSERT_TRUE(module.run());

    auto out1 = std::dynamic_pointer_cast<GpuComplexFloatSignal>(module.getData());
    ASSERT_NE(out1, nullptr);

    const auto hostOut1 = downloadGpuComplex(out1);
    ASSERT_FALSE(hostOut1.empty());

    const float halfPi = static_cast<float>(M_PI_2);
    for (const auto& z : hostOut1) {
        const float residualPhase = std::remainder(phaseOf(z), halfPi);
        const float normalizedResidual = std::abs(residualPhase) - static_cast<float>(M_PI_4);
        EXPECT_NEAR(std::abs(normalizedResidual), 0.0f, 1.0e-3f) << "block 1 residual phase too large";
    }

    ASSERT_TRUE(module.setData(makePhaseShiftedQpskSymbols(bytes2, phi2)));
    ASSERT_TRUE(module.run());

    auto out2 = std::dynamic_pointer_cast<GpuComplexFloatSignal>(module.getData());
    ASSERT_NE(out2, nullptr);

    const auto hostOut2 = downloadGpuComplex(out2);
    ASSERT_FALSE(hostOut2.empty());

    for (const auto& z : hostOut2) {
        const float residualPhase = std::remainder(phaseOf(z), halfPi);
        const float normalizedResidual = std::abs(residualPhase) - static_cast<float>(M_PI_4);
        EXPECT_NEAR(std::abs(normalizedResidual), 0.0f, 1.0e-3f) << "block 2 residual phase too large";
    }
}
