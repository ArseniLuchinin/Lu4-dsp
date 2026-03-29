#include <RRCCompute.hpp>

#include <CpuFloatSignal.hpp>
#include <EmptyContainer.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <vector>

namespace {

std::unique_ptr<RRCCompute> makeDefaultConfigured()
{
    auto module = std::make_unique<RRCCompute>();
    module->setParam("sample rate", int32_t(1'000'000));
    module->setParam("symbol rate", int32_t(250'000));
    module->setParam("rolloff", 0.35);
    module->setParam("span symbols", int32_t(8));
    module->setParam("samples per symbol", int32_t(4));
    module->setParam("normalize gain", true);
    return module;
}

std::shared_ptr<CpuFloatSignal> getSingleOutput(RRCCompute& module)
{
    auto data = module.getData();
    return std::dynamic_pointer_cast<CpuFloatSignal>(data);
}

std::vector<float> firFilter(const std::vector<float>& input, const float* taps, size_t tapsCount)
{
    std::vector<float> output(input.size(), 0.0f);
    const int mid = static_cast<int>((tapsCount - 1) / 2);

    for (size_t n = 0; n < input.size(); ++n) {
        double acc = 0.0;
        for (size_t k = 0; k < tapsCount; ++k) {
            const int idx = static_cast<int>(n) - static_cast<int>(k) + mid;
            if (idx >= 0 && idx < static_cast<int>(input.size())) {
                acc += static_cast<double>(input[static_cast<size_t>(idx)]) * static_cast<double>(taps[k]);
            }
        }
        output[n] = static_cast<float>(acc);
    }

    return output;
}

} // namespace

TEST(RRCComputeTest, InitRunAndTapCountAreValid)
{
    auto module = makeDefaultConfigured();
    ASSERT_TRUE(module->init());
    ASSERT_TRUE(module->run());

    auto taps = getSingleOutput(*module);
    ASSERT_NE(taps, nullptr);
    EXPECT_EQ(taps->size(), size_t(33));
}

TEST(RRCComputeTest, TapsAreSymmetric)
{
    auto module = makeDefaultConfigured();
    ASSERT_TRUE(module->init());

    auto taps = getSingleOutput(*module);
    ASSERT_NE(taps, nullptr);

    const size_t n = taps->size();
    const float* coeff = taps->getData();
    ASSERT_NE(coeff, nullptr);
    for (size_t i = 0; i < n / 2; ++i) {
        EXPECT_NEAR(coeff[i], coeff[n - 1 - i], 1.0e-5);
    }
}

TEST(RRCComputeTest, NormalizedGainSumIsOne)
{
    auto module = makeDefaultConfigured();
    module->setParam("normalize gain", true);
    ASSERT_TRUE(module->init());

    auto taps = getSingleOutput(*module);
    ASSERT_NE(taps, nullptr);

    double sum = 0.0;
    const float* coeff = taps->getData();
    for (size_t i = 0; i < taps->size(); ++i) {
        sum += coeff[i];
    }

    EXPECT_NEAR(sum, 1.0, 1.0e-6);
}

TEST(RRCComputeTest, RolloffChangesTapShape)
{
    auto lowAlpha = makeDefaultConfigured();
    lowAlpha->setParam("rolloff", 0.1);
    lowAlpha->setParam("normalize gain", false);
    ASSERT_TRUE(lowAlpha->init());
    auto lowTaps = getSingleOutput(*lowAlpha);
    ASSERT_NE(lowTaps, nullptr);

    auto highAlpha = makeDefaultConfigured();
    highAlpha->setParam("rolloff", 0.9);
    highAlpha->setParam("normalize gain", false);
    ASSERT_TRUE(highAlpha->init());
    auto highTaps = getSingleOutput(*highAlpha);
    ASSERT_NE(highTaps, nullptr);

    const size_t center = lowTaps->size() / 2;
    const size_t probe = center + 4;
    ASSERT_LT(probe, lowTaps->size());
    ASSERT_LT(probe, highTaps->size());

    EXPECT_GT(std::abs(lowTaps->getData()[probe] - highTaps->getData()[probe]), 1.0e-4f);
}

TEST(RRCComputeTest, SingularPointsDoNotProduceNaNOrInf)
{
    auto module = std::make_unique<RRCCompute>();
    module->setParam("sample rate", int32_t(1'000'000));
    module->setParam("symbol rate", int32_t(250'000));
    module->setParam("rolloff", 0.5);
    module->setParam("span symbols", int32_t(8));
    module->setParam("samples per symbol", int32_t(4));
    module->setParam("normalize gain", false);
    ASSERT_TRUE(module->init());

    auto taps = getSingleOutput(*module);
    ASSERT_NE(taps, nullptr);
    const float* coeff = taps->getData();
    ASSERT_NE(coeff, nullptr);
    for (size_t i = 0; i < taps->size(); ++i) {
        EXPECT_TRUE(std::isfinite(coeff[i]));
    }
}

TEST(RRCComputeTest, GetDataReturnsTapsOnlyOnceThenEmptyContainer)
{
    auto module = makeDefaultConfigured();
    ASSERT_TRUE(module->init());

    auto first = module->getData();
    auto firstTaps = std::dynamic_pointer_cast<CpuFloatSignal>(first);
    ASSERT_NE(firstTaps, nullptr);
    EXPECT_GT(firstTaps->size(), size_t(0));

    auto second = module->getData();
    auto empty = std::dynamic_pointer_cast<EmptyContainer>(second);
    ASSERT_NE(empty, nullptr);
    EXPECT_EQ(empty->size(), size_t(0));
}

TEST(RRCComputeTest, FailsWhenSampleRateIsInvalid)
{
    auto module = makeDefaultConfigured();
    module->setParam("sample rate", int32_t(0));
    EXPECT_FALSE(module->init());
}

TEST(RRCComputeTest, FailsWhenSymbolRateIsInvalid)
{
    auto module = makeDefaultConfigured();
    module->setParam("symbol rate", int32_t(0));
    EXPECT_FALSE(module->init());
}

TEST(RRCComputeTest, FailsWhenRolloffIsOutOfRange)
{
    auto module = makeDefaultConfigured();
    module->setParam("rolloff", 1.2);
    EXPECT_FALSE(module->init());
}

TEST(RRCComputeTest, FailsWhenSpanIsInvalid)
{
    auto module = makeDefaultConfigured();
    module->setParam("span symbols", int32_t(0));
    EXPECT_FALSE(module->init());
}

TEST(RRCComputeTest, FailsWhenSamplesPerSymbolIsInvalid)
{
    auto module = makeDefaultConfigured();
    module->setParam("samples per symbol", int32_t(1));
    EXPECT_FALSE(module->init());
}

TEST(RRCComputeTest, FailsForNonIntegerSampleToSymbolRateRatioWhenSpsNotSet)
{
    auto module = std::make_unique<RRCCompute>();
    module->setParam("sample rate", int32_t(1'000'000));
    module->setParam("symbol rate", int32_t(300'000));
    module->setParam("rolloff", 0.35);
    module->setParam("span symbols", int32_t(8));
    module->setParam("normalize gain", true);
    EXPECT_FALSE(module->init());
}

TEST(RRCComputeTest, FilteringConstantSignalConvergesToUnityGain)
{
    auto module = makeDefaultConfigured();
    module->setParam("normalize gain", true);
    ASSERT_TRUE(module->init());

    auto taps = getSingleOutput(*module);
    ASSERT_NE(taps, nullptr);
    ASSERT_GT(taps->size(), size_t(0));

    std::vector<float> input(512, 1.0f);
    const auto output = firFilter(input, taps->getData(), taps->size());

    // Ignore transient edges and check steady-state gain near 1.0.
    const size_t edge = taps->size();
    ASSERT_LT(edge * 2, output.size());
    for (size_t i = edge; i < output.size() - edge; ++i) {
        EXPECT_NEAR(output[i], 1.0f, 2.0e-3f);
    }
}
