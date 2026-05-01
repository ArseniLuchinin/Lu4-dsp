#include <IModule.hpp>
#include <Variables.hpp>
#include <VirtualTransmitter.hpp>
#include <EmptyContainer.hpp>

#include <gtest/gtest.h>

#include <any>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>

namespace {

class FetchParamProbeModule : public IModule {
public:
    FetchParamProbeModule()
        : IModule({"FetchParamProbeModule", "", ""})
    {}

    bool init() override {
        return true;
    }

    bool run() override {
        return true;
    }

    void setParam(const std::string& paramName, const std::any& value) override {
        m_lastParamName = paramName;
        m_lastValue = value;
    }

    bool setData(std::shared_ptr<IData> data) override {
        (void)data;
        return true;
    }

    std::shared_ptr<IData> getData() override {
        return nullptr;
    }

    std::string m_lastParamName;
    std::any m_lastValue;
};

} // namespace

TEST(FetchParamTests, TagTokenFetchesDataAndPassesSharedPtrToSetParam)
{
    VirtualTransmitter::setTimeoutMs(2000);

    FetchParamProbeModule module;
    const std::string tag = "fetch_param_tag_token";
    auto txData = std::make_shared<EmptyContainer>();
    ASSERT_NE(txData, nullptr);

    std::thread fetchThread([&module, &tag]() {
        module.fetchParam("taps", std::string("@") + tag);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    VirtualTransmitter transmitter;
    transmitter.txData(txData, tag);

    fetchThread.join();

    EXPECT_EQ(module.m_lastParamName, "taps");
    const auto fetchedData = std::any_cast<std::shared_ptr<IData>>(&module.m_lastValue);
    ASSERT_NE(fetchedData, nullptr);
    ASSERT_NE(*fetchedData, nullptr);
    EXPECT_EQ(*fetchedData, txData);
}

TEST(FetchParamTests, EmptyTagTokenPassesEmptyAnyToSetParam)
{
    FetchParamProbeModule module;

    module.fetchParam("taps", std::string("@"));

    EXPECT_EQ(module.m_lastParamName, "taps");
    EXPECT_FALSE(module.m_lastValue.has_value());
}

TEST(FetchParamTests, VariableTokenStillResolvedThroughVariables)
{
    FetchParamProbeModule module;
    const auto variablesPath = std::filesystem::temp_directory_path() / "fetch_param_tests_variables.toml";

    {
        std::ofstream file(variablesPath);
        ASSERT_TRUE(file.is_open());
        file << "FETCH_PARAM_ORDER = 123\n";
        ASSERT_TRUE(file.good());
    }

    ASSERT_TRUE(Variables::instance().load(variablesPath.string()));

    module.fetchParam("order", std::string("$FETCH_PARAM_ORDER"));

    EXPECT_EQ(module.m_lastParamName, "order");
    const auto parsedValue = std::any_cast<int64_t>(&module.m_lastValue);
    ASSERT_NE(parsedValue, nullptr);
    EXPECT_EQ(*parsedValue, static_cast<int64_t>(123));
}
