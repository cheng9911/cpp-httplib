#include <gtest/gtest.h>
#include "rpf/plugin_manager.h"
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

class PluginManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/rpf_test_plugins";
        fs::create_directories(test_dir_);
    }

    void TearDown() override {
        fs::remove_all(test_dir_);
    }

    std::string test_dir_;
};

TEST_F(PluginManagerTest, ScanEmptyDirectory) {
    rpf::PluginManager manager(test_dir_);
    auto plugins = manager.scanAvailablePlugins();
    EXPECT_TRUE(plugins.empty());
}

TEST_F(PluginManagerTest, ScanWithMetadata) {
    // 创建测试元数据文件
    nlohmann::json meta = {
        {"name", "test_plugin"},
        {"version", "1.0.0"},
        {"description", "Test"},
        {"author", "Test"},
        {"license", "MIT"},
        {"dependencies", nlohmann::json::array()},
        {"provides", {"Service1"}},
        {"category", "test"},
        {"platform", "all"},
        {"entry_point", "create_plugin"}
    };

    std::string meta_file = test_dir_ + "/test_plugin.meta.json";
    {
        std::ofstream ofs(meta_file);
        ofs << meta.dump(4);
    }

    rpf::PluginManager manager(test_dir_);
    auto plugins = manager.scanAvailablePlugins();

    EXPECT_EQ(plugins.size(), 1);
    EXPECT_EQ(plugins[0].name, "test_plugin");
    EXPECT_EQ(plugins[0].version, "1.0.0");
}

TEST_F(PluginManagerTest, GetPluginState) {
    rpf::PluginManager manager(test_dir_);
    EXPECT_EQ(manager.getPluginState("nonexistent"), rpf::PluginState::Unloaded);
}

TEST_F(PluginManagerTest, GetLoadedPluginsEmpty) {
    rpf::PluginManager manager(test_dir_);
    auto loaded = manager.getLoadedPlugins();
    EXPECT_TRUE(loaded.empty());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
