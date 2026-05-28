#include <gtest/gtest.h>
#include "rpf/plugin_metadata.h"
#include <fstream>

class MetadataTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建测试元数据文件
        test_meta_ = {
            {"name", "test_plugin"},
            {"version", "1.0.0"},
            {"description", "Test plugin"},
            {"author", "Test Author"},
            {"license", "MIT"},
            {"dependencies", nlohmann::json::array()},
            {"provides", {"Service1", "Service2"}},
            {"category", "test"},
            {"platform", "all"},
            {"entry_point", "create_plugin"}
        };
    }

    nlohmann::json test_meta_;
};

TEST_F(MetadataTest, JsonSerialization) {
    rpf::PluginMetadata meta;
    meta.name = "test_plugin";
    meta.version = "1.0.0";
    meta.description = "Test plugin";
    meta.author = "Test Author";
    meta.license = "MIT";
    meta.provides = {"Service1", "Service2"};
    meta.category = "test";
    meta.platform = "all";

    nlohmann::json j = meta;
    EXPECT_EQ(j["name"], "test_plugin");
    EXPECT_EQ(j["version"], "1.0.0");
    EXPECT_EQ(j["provides"].size(), 2);
}

TEST_F(MetadataTest, JsonDeserialization) {
    rpf::PluginMetadata meta = test_meta_.get<rpf::PluginMetadata>();
    EXPECT_EQ(meta.name, "test_plugin");
    EXPECT_EQ(meta.version, "1.0.0");
    EXPECT_EQ(meta.provides.size(), 2);
    EXPECT_EQ(meta.provides[0], "Service1");
}

TEST_F(MetadataTest, LoadFromFile) {
    std::string temp_file = "/tmp/test_meta.json";
    {
        std::ofstream ofs(temp_file);
        ofs << test_meta_.dump(4);
    }

    auto meta = rpf::loadMetadataFromFile(temp_file);
    EXPECT_EQ(meta.name, "test_plugin");
    EXPECT_EQ(meta.version, "1.0.0");

    std::remove(temp_file.c_str());
}

TEST_F(MetadataTest, DependencySerialization) {
    rpf::Dependency dep;
    dep.name = "other_plugin";
    dep.version_constraint = ">=1.0.0";
    dep.required = true;

    nlohmann::json j = dep;
    EXPECT_EQ(j["name"], "other_plugin");
    EXPECT_EQ(j["version_constraint"], ">=1.0.0");
    EXPECT_EQ(j["required"], true);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
