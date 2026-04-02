#include <CarrierPhaseEstimator.hpp>

#include <GpuComplexSignal.hpp>
#include <GpuFloatSignal.hpp>
#include <VirtualTransmitter.hpp>

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

float downloadSingleGpuFloat(const std::shared_ptr<GpuFloatSignal>& data, bool* ok = nullptr)
{
    if (ok) {
        *ok = false;
    }
    if (!data || !data->isValid() || data->size() == 0 || !data->getDeviceData()) {
        return 0.0f;
    }

    float value = 0.0f;
    const auto err = cudaMemcpy(&value, data->getDeviceData(), sizeof(float), cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        return 0.0f;
    }

    if (ok) {
        *ok = true;
    }
    return value;
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

std::shared_ptr<GpuComplexFloatSignal> scaleSymbols(
    const std::shared_ptr<GpuComplexFloatSignal>& in,
    const std::vector<float>& amplitudes)
{
    if (!in || !in->getDeviceData()) {
        return std::make_shared<GpuComplexFloatSignal>();
    }

    std::vector<cuComplex> host = downloadGpuComplex(in);
    const size_t n = host.size();
    if (n == 0 && in->size() != 0) {
        return std::make_shared<GpuComplexFloatSignal>();
    }

    for (size_t i = 0; i < n; ++i) {
        const float amp = amplitudes.empty() ? 1.0f : amplitudes[i % amplitudes.size()];
        host[i].x *= amp;
        host[i].y *= amp;
    }

    std::vector<std::pair<float, float>> out;
    out.reserve(n);
    for (const auto& s : host) {
        out.emplace_back(s.x, s.y);
    }
    return makeGpuComplex(out);
}

} // namespace

TEST(CarrierPhaseEstimatorModuleTest, UsesFourthPower)
{
    if (!isCudaAvailable()) {
        GTEST_SKIP() << "CUDA device is unavailable in test environment.";
    }

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

    ASSERT_TRUE(module.setData(makeGpuComplex(symbols)));
    ASSERT_TRUE(module.run());

    VirtualTransmitter tx;
    auto phaseData = std::dynamic_pointer_cast<GpuFloatSignal>(tx.waitRxData("phase_test_tag_1"));
    ASSERT_NE(phaseData, nullptr);
    ASSERT_EQ(phaseData->size(), size_t(1));

    bool phaseOk = false;
    const float measured = downloadSingleGpuFloat(phaseData, &phaseOk);
    ASSERT_TRUE(phaseOk);
    const float wrappedErr = std::remainder(measured - phi, static_cast<float>(M_PI_2));
    EXPECT_NEAR(wrappedErr, 0.0f, 1.0e-3f);
}

TEST(CarrierPhaseEstimatorModuleTest, EmptyInputProducesZeroPhase)
{
    if (!isCudaAvailable()) {
        GTEST_SKIP() << "CUDA device is unavailable in test environment.";
    }

    CarrierPhaseEstimator module;
    module.setParam("phase tag", std::string("phase_test_tag_empty"));
    ASSERT_TRUE(module.init());

    ASSERT_TRUE(module.setData(makeGpuComplex({})));
    ASSERT_TRUE(module.run());

    VirtualTransmitter tx;
    auto phaseData = std::dynamic_pointer_cast<GpuFloatSignal>(tx.waitRxData("phase_test_tag_empty"));
    ASSERT_NE(phaseData, nullptr);
    ASSERT_EQ(phaseData->size(), size_t(1));

    bool phaseOk = false;
    EXPECT_FLOAT_EQ(downloadSingleGpuFloat(phaseData, &phaseOk), 0.0f);
    ASSERT_TRUE(phaseOk);
}

TEST(CarrierPhaseEstimatorModuleTest, AmplitudeImbalanceStillEstimatesPhase)
{
    if (!isCudaAvailable()) {
        GTEST_SKIP() << "CUDA device is unavailable in test environment.";
    }

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
    auto phaseData = std::dynamic_pointer_cast<GpuFloatSignal>(tx.waitRxData("phase_test_tag_amp_imbalance"));
    ASSERT_NE(phaseData, nullptr);
    ASSERT_EQ(phaseData->size(), size_t(1));

    bool phaseOk = false;
    const float measured = downloadSingleGpuFloat(phaseData, &phaseOk);
    ASSERT_TRUE(phaseOk);
    const float wrappedErr = std::remainder(measured - phi, static_cast<float>(M_PI_2));
    EXPECT_NEAR(wrappedErr, 0.0f, 3.0e-3f);
}

TEST(CarrierPhaseEstimatorModuleTest, PhaseWrapAroundNearPiOver4)
{
    if (!isCudaAvailable()) {
        GTEST_SKIP() << "CUDA device is unavailable in test environment.";
    }

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

        auto phaseData = std::dynamic_pointer_cast<GpuFloatSignal>(tx.waitRxData("phase_test_tag_wrap"));
        ASSERT_NE(phaseData, nullptr);
        ASSERT_EQ(phaseData->size(), size_t(1));

        bool phaseOk = false;
        const float measured = downloadSingleGpuFloat(phaseData, &phaseOk);
        ASSERT_TRUE(phaseOk);
        const float wrappedErr = std::remainder(measured - phi, static_cast<float>(M_PI_2));
        EXPECT_NEAR(wrappedErr, 0.0f, 2.0e-3f);
    }
}
