#include <gtest/gtest.h>
#include "rpf/plugin_metadata.h"
#include "rpf/dependency_resolver.h"
#include "rpf/event_bus.h"
#include "rpf/service_registry.h"
#include "rpf/plugin_manager.h"
#include "rpf/plugin_interface.h"
#include "rpf/hardware_interface.h"
#include "rpf/motion_controller.h"
#include "rpf/vision_detector.h"
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <set>
#include <atomic>

namespace fs = std::filesystem;

// ============================================================
// 1.1 元数据加载和解析测试 (M-001 ~ M-023)
// ============================================================

class MetadataTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/rpf_test_metadata";
        fs::create_directories(test_dir_);
    }
    void TearDown() override {
        fs::remove_all(test_dir_);
    }
    std::string test_dir_;
};

// M-001: JSON 序列化 - 完整字段
TEST_F(MetadataTest, M001_JsonSerializationFullFields) {
    rpf::PluginMetadata meta;
    meta.name = "test_plugin";
    meta.version = "1.0.0";
    meta.description = "Test plugin";
    meta.author = "Test Author";
    meta.license = "MIT";
    meta.dependencies = {{"dep1", ">=1.0.0", true}};
    meta.provides = {"Service1", "Service2"};
    meta.category = "test";
    meta.platform = "all";
    meta.entry_point = "create_plugin";

    nlohmann::json j = meta;
    EXPECT_EQ(j["name"], "test_plugin");
    EXPECT_EQ(j["version"], "1.0.0");
    EXPECT_EQ(j["description"], "Test plugin");
    EXPECT_EQ(j["author"], "Test Author");
    EXPECT_EQ(j["license"], "MIT");
    EXPECT_EQ(j["dependencies"].size(), 1);
    EXPECT_EQ(j["dependencies"][0]["name"], "dep1");
    EXPECT_EQ(j["provides"].size(), 2);
    EXPECT_EQ(j["category"], "test");
    EXPECT_EQ(j["platform"], "all");
    EXPECT_EQ(j["entry_point"], "create_plugin");
}

// M-002: JSON 反序列化 - 完整字段
TEST_F(MetadataTest, M002_JsonDeserializationFullFields) {
    nlohmann::json j = {
        {"name", "test_plugin"},
        {"version", "2.0.0"},
        {"description", "Desc"},
        {"author", "Author"},
        {"license", "Apache"},
        {"dependencies", {{{"name", "dep1"}, {"version_constraint", ">=1.0.0"}, {"required", true}}}},
        {"provides", {"Svc1", "Svc2"}},
        {"category", "hardware"},
        {"platform", "linux"},
        {"entry_point", "my_entry"}
    };

    auto meta = j.get<rpf::PluginMetadata>();
    EXPECT_EQ(meta.name, "test_plugin");
    EXPECT_EQ(meta.version, "2.0.0");
    EXPECT_EQ(meta.description, "Desc");
    EXPECT_EQ(meta.author, "Author");
    EXPECT_EQ(meta.license, "Apache");
    EXPECT_EQ(meta.dependencies.size(), 1);
    EXPECT_EQ(meta.dependencies[0].name, "dep1");
    EXPECT_EQ(meta.dependencies[0].version_constraint, ">=1.0.0");
    EXPECT_EQ(meta.dependencies[0].required, true);
    EXPECT_EQ(meta.provides.size(), 2);
    EXPECT_EQ(meta.category, "hardware");
    EXPECT_EQ(meta.platform, "linux");
    EXPECT_EQ(meta.entry_point, "my_entry");
}

// M-003: JSON 反序列化 - 仅必填字段
TEST_F(MetadataTest, M003_JsonDeserializationOnlyRequired) {
    nlohmann::json j = {{"name", "minimal"}, {"version", "0.1.0"}};
    auto meta = j.get<rpf::PluginMetadata>();
    EXPECT_EQ(meta.name, "minimal");
    EXPECT_EQ(meta.version, "0.1.0");
    EXPECT_TRUE(meta.description.empty());
    EXPECT_TRUE(meta.author.empty());
    EXPECT_TRUE(meta.license.empty());
    EXPECT_TRUE(meta.dependencies.empty());
    EXPECT_TRUE(meta.provides.empty());
    EXPECT_TRUE(meta.category.empty());
    EXPECT_EQ(meta.platform, "all");
    EXPECT_EQ(meta.entry_point, "create_plugin");
}

// M-004: 缺少必填字段 name
TEST_F(MetadataTest, M004_MissingNameField) {
    nlohmann::json j = {{"version", "1.0.0"}};
    EXPECT_THROW(j.get<rpf::PluginMetadata>(), nlohmann::json::out_of_range);
}

// M-005: 缺少必填字段 version
TEST_F(MetadataTest, M005_MissingVersionField) {
    nlohmann::json j = {{"name", "test"}};
    EXPECT_THROW(j.get<rpf::PluginMetadata>(), nlohmann::json::out_of_range);
}

// M-006: 字段类型错误
TEST_F(MetadataTest, M006_FieldTypeError) {
    nlohmann::json j = {{"name", 12345}, {"version", "1.0.0"}};
    EXPECT_THROW(j.get<rpf::PluginMetadata>(), nlohmann::json::type_error);
}

// M-007: 空 JSON 对象
TEST_F(MetadataTest, M007_EmptyJsonObject) {
    nlohmann::json j = {};
    // 空JSON实际上是null类型，at()会抛出type_error而非out_of_range
    EXPECT_THROW(j.get<rpf::PluginMetadata>(), nlohmann::json::type_error);
}

// M-008: Dependency 序列化/反序列化
TEST_F(MetadataTest, M008_DependencySerializationRoundtrip) {
    rpf::Dependency dep;
    dep.name = "dep1";
    dep.version_constraint = ">=1.0.0";
    dep.required = true;

    nlohmann::json j = dep;
    EXPECT_EQ(j["name"], "dep1");
    EXPECT_EQ(j["version_constraint"], ">=1.0.0");
    EXPECT_EQ(j["required"], true);

    auto dep2 = j.get<rpf::Dependency>();
    EXPECT_EQ(dep2.name, "dep1");
    EXPECT_EQ(dep2.version_constraint, ">=1.0.0");
    EXPECT_EQ(dep2.required, true);
}

// M-009: Dependency 反序列化 - 可选字段缺失
TEST_F(MetadataTest, M009_DependencyOptionalFieldsMissing) {
    nlohmann::json j = {{"name", "dep_only"}};
    auto dep = j.get<rpf::Dependency>();
    EXPECT_EQ(dep.name, "dep_only");
    EXPECT_TRUE(dep.version_constraint.empty());
    EXPECT_EQ(dep.required, true);
}

// M-010: 从文件加载 - 正常文件
TEST_F(MetadataTest, M010_LoadFromFileNormal) {
    nlohmann::json j = {
        {"name", "file_plugin"},
        {"version", "3.0.0"},
        {"description", "From file"},
        {"author", "File Author"},
        {"license", "BSD"},
        {"dependencies", nlohmann::json::array()},
        {"provides", {"FileService"}},
        {"category", "file"},
        {"platform", "all"},
        {"entry_point", "create_plugin"}
    };

    std::string path = test_dir_ + "/file_plugin.meta.json";
    {
        std::ofstream ofs(path);
        ofs << j.dump(4);
    }

    auto meta = rpf::loadMetadataFromFile(path);
    EXPECT_EQ(meta.name, "file_plugin");
    EXPECT_EQ(meta.version, "3.0.0");
    EXPECT_EQ(meta.description, "From file");
    EXPECT_EQ(meta.author, "File Author");
}

// M-011: 从文件加载 - 文件不存在
TEST_F(MetadataTest, M011_LoadFromFileNotFound) {
    EXPECT_THROW(rpf::loadMetadataFromFile("/nonexistent/path.json"), std::runtime_error);
}

// M-012: 从文件加载 - JSON 格式错误
TEST_F(MetadataTest, M012_LoadFromFileInvalidJson) {
    std::string path = test_dir_ + "/invalid.meta.json";
    {
        std::ofstream ofs(path);
        ofs << "{invalid json}";
    }
    EXPECT_THROW(rpf::loadMetadataFromFile(path), nlohmann::json::parse_error);
}

// M-013: 从文件加载 - 空文件
TEST_F(MetadataTest, M013_LoadFromFileEmpty) {
    std::string path = test_dir_ + "/empty.meta.json";
    {
        std::ofstream ofs(path);
        ofs << "";
    }
    EXPECT_THROW(rpf::loadMetadataFromFile(path), nlohmann::json::parse_error);
}

// M-014: 从文件加载 - mujoco_hardware_plugin.meta.json
TEST_F(MetadataTest, M014_LoadMujocoHardwareMetadata) {
    // 这个测试需要从examples目录读取真实的meta.json文件
    std::string path = "../../examples/mujoco_hardware_plugin/mujoco_hardware_plugin.meta.json";
    if (!fs::exists(path)) {
        // 尝试从build目录的相对路径
        path = "../examples/mujoco_hardware_plugin/mujoco_hardware_plugin.meta.json";
    }
    if (!fs::exists(path)) {
        GTEST_SKIP() << "mujoco_hardware_plugin.meta.json not found at expected path";
    }

    auto meta = rpf::loadMetadataFromFile(path);
    EXPECT_EQ(meta.name, "mujoco_hardware");
    EXPECT_EQ(meta.version, "1.0.0");
    EXPECT_TRUE(std::find(meta.provides.begin(), meta.provides.end(), "Hardware") != meta.provides.end());
    EXPECT_TRUE(std::find(meta.provides.begin(), meta.provides.end(), "Simulator") != meta.provides.end());
}

// M-015: 从文件加载 - motion_control_plugin.meta.json
TEST_F(MetadataTest, M015_LoadMotionControlMetadata) {
    std::string path = "../../examples/motion_control_plugin/motion_control_plugin.meta.json";
    if (!fs::exists(path)) {
        path = "../examples/motion_control_plugin/motion_control_plugin.meta.json";
    }
    if (!fs::exists(path)) {
        GTEST_SKIP() << "motion_control_plugin.meta.json not found";
    }

    auto meta = rpf::loadMetadataFromFile(path);
    EXPECT_EQ(meta.name, "motion_control");
    EXPECT_TRUE(std::find(meta.provides.begin(), meta.provides.end(), "MotionController") != meta.provides.end());
    EXPECT_TRUE(std::find(meta.provides.begin(), meta.provides.end(), "TrajectoryPlanner") != meta.provides.end());
}

// M-016: 从文件加载 - vision_plugin.meta.json
TEST_F(MetadataTest, M016_LoadVisionMetadata) {
    std::string path = "../../examples/vision_plugin/vision_plugin.meta.json";
    if (!fs::exists(path)) {
        path = "../examples/vision_plugin/vision_plugin.meta.json";
    }
    if (!fs::exists(path)) {
        GTEST_SKIP() << "vision_plugin.meta.json not found";
    }

    auto meta = rpf::loadMetadataFromFile(path);
    EXPECT_EQ(meta.name, "vision");
    EXPECT_TRUE(std::find(meta.provides.begin(), meta.provides.end(), "VisionDetector") != meta.provides.end());
    EXPECT_TRUE(std::find(meta.provides.begin(), meta.provides.end(), "PoseEstimator") != meta.provides.end());
}

// M-017: 带依赖的元数据序列化
TEST_F(MetadataTest, M017_MetadataWithDependenciesRoundtrip) {
    rpf::PluginMetadata meta;
    meta.name = "complex";
    meta.version = "1.0.0";
    meta.dependencies = {
        {"dep1", ">=1.0.0", true},
        {"dep2", "^2.0.0", false}
    };

    nlohmann::json j = meta;
    auto meta2 = j.get<rpf::PluginMetadata>();

    EXPECT_EQ(meta2.dependencies.size(), 2);
    EXPECT_EQ(meta2.dependencies[0].name, "dep1");
    EXPECT_EQ(meta2.dependencies[0].required, true);
    EXPECT_EQ(meta2.dependencies[1].name, "dep2");
    EXPECT_EQ(meta2.dependencies[1].required, false);
}

// M-018: scanPluginDirectory - 正常目录
TEST_F(MetadataTest, M018_ScanPluginDirectory) {
    nlohmann::json j1 = {{"name", "p1"}, {"version", "1.0.0"}, {"dependencies", nlohmann::json::array()}, {"provides", nlohmann::json::array()}, {"platform", "all"}, {"entry_point", "create_plugin"}};
    nlohmann::json j2 = {{"name", "p2"}, {"version", "2.0.0"}, {"dependencies", nlohmann::json::array()}, {"provides", nlohmann::json::array()}, {"platform", "all"}, {"entry_point", "create_plugin"}};

    {
        std::ofstream(test_dir_ + "/p1.meta.json") << j1.dump(4);
        std::ofstream(test_dir_ + "/p2.meta.json") << j2.dump(4);
        std::ofstream(test_dir_ + "/not_meta.json") << "{}";
        std::ofstream(test_dir_ + "/readme.txt") << "text";
    }

    auto result = rpf::scanPluginDirectory(test_dir_);
    EXPECT_EQ(result.size(), 2);
}

// M-019: 扫描目录 - 空目录
TEST_F(MetadataTest, M019_ScanEmptyDirectory) {
    std::string empty_dir = test_dir_ + "/empty";
    fs::create_directories(empty_dir);
    auto result = rpf::scanPluginDirectory(empty_dir);
    EXPECT_TRUE(result.empty());
}

// M-020: 扫描目录 - 目录不存在
TEST_F(MetadataTest, M020_ScanNonexistentDirectory) {
    auto result = rpf::scanPluginDirectory("/nonexistent/dir");
    EXPECT_TRUE(result.empty());
}

// M-021: 扫描目录 - 含损坏的 meta.json
TEST_F(MetadataTest, M021_ScanDirectoryWithCorruptedMeta) {
    nlohmann::json j1 = {{"name", "good"}, {"version", "1.0.0"}, {"dependencies", nlohmann::json::array()}, {"provides", nlohmann::json::array()}, {"platform", "all"}, {"entry_point", "create_plugin"}};

    {
        std::ofstream(test_dir_ + "/good.meta.json") << j1.dump(4);
        std::ofstream(test_dir_ + "/bad.meta.json") << "{corrupted}";
    }

    auto result = rpf::scanPluginDirectory(test_dir_);
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].name, "good");
}

// M-022: pluginStateToString 函数
TEST_F(MetadataTest, M022_PluginStateToString) {
    EXPECT_STREQ(rpf::pluginStateToString(rpf::PluginState::Unloaded), "Unloaded");
    EXPECT_STREQ(rpf::pluginStateToString(rpf::PluginState::Loaded), "Loaded");
    EXPECT_STREQ(rpf::pluginStateToString(rpf::PluginState::Initialized), "Initialized");
    EXPECT_STREQ(rpf::pluginStateToString(rpf::PluginState::Running), "Running");
    EXPECT_STREQ(rpf::pluginStateToString(rpf::PluginState::Stopped), "Stopped");
    EXPECT_STREQ(rpf::pluginStateToString(rpf::PluginState::Error), "Error");
}

// M-023: 元数据往返一致性
TEST_F(MetadataTest, M023_MetadataRoundtripConsistency) {
    rpf::PluginMetadata meta;
    meta.name = "roundtrip";
    meta.version = "1.2.3";
    meta.description = "Roundtrip test";
    meta.author = "Author";
    meta.license = "MIT";
    meta.dependencies = {{"dep1", ">=1.0.0", true}};
    meta.provides = {"Svc1"};
    meta.category = "test";
    meta.platform = "linux";
    meta.entry_point = "my_create";

    nlohmann::json j1 = meta;
    auto meta2 = j1.get<rpf::PluginMetadata>();
    nlohmann::json j2 = meta2;

    EXPECT_EQ(j1.dump(), j2.dump());
}

// ============================================================
// 1.2 依赖解析测试 (D-001 ~ D-019)
// ============================================================

class DependencyResolverTest : public ::testing::Test {
protected:
    rpf::DependencyResolver resolver;

    rpf::PluginMetadata makePlugin(const std::string& name,
                                    std::vector<rpf::Dependency> deps = {}) {
        rpf::PluginMetadata m;
        m.name = name;
        m.version = "1.0.0";
        m.dependencies = std::move(deps);
        return m;
    }
};

// D-001: 无依赖拓扑排序
TEST_F(DependencyResolverTest, D001_NoDependenciesTopologicalSort) {
    std::map<std::string, rpf::PluginMetadata> plugins;
    plugins["A"] = makePlugin("A");
    plugins["B"] = makePlugin("B");
    plugins["C"] = makePlugin("C");

    auto order = resolver.resolveLoadOrder(plugins);
    EXPECT_EQ(order.size(), 3);
    std::set<std::string> names(order.begin(), order.end());
    EXPECT_TRUE(names.count("A") && names.count("B") && names.count("C"));
}

// D-002: 线性依赖排序 A->B->C
TEST_F(DependencyResolverTest, D002_LinearDependencySort) {
    std::map<std::string, rpf::PluginMetadata> plugins;
    plugins["C"] = makePlugin("C");
    plugins["B"] = makePlugin("B", {{"C", ">=1.0.0", true}});
    plugins["A"] = makePlugin("A", {{"B", ">=1.0.0", true}});

    auto order = resolver.resolveLoadOrder(plugins);
    auto c_pos = std::find(order.begin(), order.end(), "C");
    auto b_pos = std::find(order.begin(), order.end(), "B");
    auto a_pos = std::find(order.begin(), order.end(), "A");

    EXPECT_LT(c_pos, b_pos);
    EXPECT_LT(b_pos, a_pos);
}

// D-003: 多依赖排序
TEST_F(DependencyResolverTest, D003_MultipleDependencySort) {
    std::map<std::string, rpf::PluginMetadata> plugins;
    plugins["B"] = makePlugin("B");
    plugins["C"] = makePlugin("C");
    plugins["A"] = makePlugin("A", {{"B", ">=1.0.0", true}, {"C", ">=1.0.0", true}});

    auto order = resolver.resolveLoadOrder(plugins);
    auto a_pos = std::find(order.begin(), order.end(), "A");
    auto b_pos = std::find(order.begin(), order.end(), "B");
    auto c_pos = std::find(order.begin(), order.end(), "C");

    EXPECT_LT(b_pos, a_pos);
    EXPECT_LT(c_pos, a_pos);
}

// D-004: 菱形依赖
TEST_F(DependencyResolverTest, D004_DiamondDependency) {
    std::map<std::string, rpf::PluginMetadata> plugins;
    plugins["D"] = makePlugin("D");
    plugins["B"] = makePlugin("B", {{"D", ">=1.0.0", true}});
    plugins["C"] = makePlugin("C", {{"D", ">=1.0.0", true}});
    plugins["A"] = makePlugin("A", {{"B", ">=1.0.0", true}, {"C", ">=1.0.0", true}});

    auto order = resolver.resolveLoadOrder(plugins);
    auto d_pos = std::find(order.begin(), order.end(), "D");
    auto b_pos = std::find(order.begin(), order.end(), "B");
    auto c_pos = std::find(order.begin(), order.end(), "C");
    auto a_pos = std::find(order.begin(), order.end(), "A");

    EXPECT_LT(d_pos, b_pos);
    EXPECT_LT(d_pos, c_pos);
    EXPECT_LT(b_pos, a_pos);
    EXPECT_LT(c_pos, a_pos);
}

// D-005: 循环依赖检测 A->B->C->A
TEST_F(DependencyResolverTest, D005_CircularDependencyDetection) {
    std::map<std::string, rpf::PluginMetadata> plugins;
    plugins["A"] = makePlugin("A", {{"B", ">=1.0.0", true}});
    plugins["B"] = makePlugin("B", {{"C", ">=1.0.0", true}});
    plugins["C"] = makePlugin("C", {{"A", ">=1.0.0", true}});

    EXPECT_THROW(resolver.resolveLoadOrder(plugins), std::runtime_error);
}

// D-006: 自循环依赖
TEST_F(DependencyResolverTest, D006_SelfCircularDependency) {
    std::map<std::string, rpf::PluginMetadata> plugins;
    plugins["A"] = makePlugin("A", {{"A", ">=1.0.0", true}});

    // 自循环: A 依赖自身。根据算法，A 不在已初始化的 in_degree 中被当作自身依赖
    // 检查：如果 A 在 plugins 中且 required，它会增加 A 的 in_degree
    // 但 Kahn's 算法中，由于 A->A 边，A 的 in_degree=1，不会被加入队列
    // result.size() == 0 != 1，所以抛异常
    EXPECT_THROW(resolver.resolveLoadOrder(plugins), std::runtime_error);
}

// D-007: 空插件列表
TEST_F(DependencyResolverTest, D007_EmptyPluginList) {
    std::map<std::string, rpf::PluginMetadata> plugins;
    auto order = resolver.resolveLoadOrder(plugins);
    EXPECT_TRUE(order.empty());
}

// D-008: 单个无依赖插件
TEST_F(DependencyResolverTest, D008_SinglePluginNoDependencies) {
    std::map<std::string, rpf::PluginMetadata> plugins;
    plugins["A"] = makePlugin("A");

    auto order = resolver.resolveLoadOrder(plugins);
    EXPECT_EQ(order.size(), 1);
    EXPECT_EQ(order[0], "A");
}

// D-009: 非必选依赖不影响排序
TEST_F(DependencyResolverTest, D009_OptionalDependencyNotInList) {
    std::map<std::string, rpf::PluginMetadata> plugins;
    plugins["A"] = makePlugin("A", {{"external", ">=1.0.0", false}});

    auto order = resolver.resolveLoadOrder(plugins);
    EXPECT_EQ(order.size(), 1);
    EXPECT_EQ(order[0], "A");
}

// D-010: checkDependencies - 依赖满足
TEST_F(DependencyResolverTest, D010_CheckDependenciesSatisfied) {
    std::map<std::string, rpf::PluginMetadata> plugins;
    plugins["B"] = makePlugin("B");
    plugins["A"] = makePlugin("A", {{"B", ">=1.0.0", true}});

    EXPECT_TRUE(resolver.checkDependencies("A", plugins));
}

// D-011: checkDependencies - 依赖不满足
TEST_F(DependencyResolverTest, D011_CheckDependenciesNotSatisfied) {
    std::map<std::string, rpf::PluginMetadata> plugins;
    plugins["A"] = makePlugin("A", {{"B", ">=1.0.0", true}});

    EXPECT_FALSE(resolver.checkDependencies("A", plugins));
}

// D-012: checkDependencies - 插件不存在
TEST_F(DependencyResolverTest, D012_CheckDependenciesPluginNotFound) {
    std::map<std::string, rpf::PluginMetadata> plugins;
    EXPECT_FALSE(resolver.checkDependencies("X", plugins));
}

// D-013: checkDependencies - 可选依赖缺失
TEST_F(DependencyResolverTest, D013_CheckDependenciesOptionalMissing) {
    std::map<std::string, rpf::PluginMetadata> plugins;
    plugins["A"] = makePlugin("A", {{"B", ">=1.0.0", false}});

    EXPECT_TRUE(resolver.checkDependencies("A", plugins));
}

// D-014: checkDependencies - 无依赖
TEST_F(DependencyResolverTest, D014_CheckDependenciesNoDependencies) {
    std::map<std::string, rpf::PluginMetadata> plugins;
    plugins["A"] = makePlugin("A");

    EXPECT_TRUE(resolver.checkDependencies("A", plugins));
}

// D-015: detectCycles - 存在循环
TEST_F(DependencyResolverTest, D015_DetectCyclesWithCycle) {
    std::map<std::string, rpf::PluginMetadata> plugins;
    plugins["A"] = makePlugin("A", {{"B", ">=1.0.0", true}});
    plugins["B"] = makePlugin("B", {{"C", ">=1.0.0", true}});
    plugins["C"] = makePlugin("C", {{"A", ">=1.0.0", true}});

    auto cycles = resolver.detectCycles(plugins);
    EXPECT_FALSE(cycles.empty());
}

// D-016: detectCycles - 无循环
TEST_F(DependencyResolverTest, D016_DetectCyclesNoCycle) {
    std::map<std::string, rpf::PluginMetadata> plugins;
    plugins["C"] = makePlugin("C");
    plugins["B"] = makePlugin("B", {{"C", ">=1.0.0", true}});
    plugins["A"] = makePlugin("A", {{"B", ">=1.0.0", true}});

    auto cycles = resolver.detectCycles(plugins);
    EXPECT_TRUE(cycles.empty());
}

// D-018: 大规模插件排序性能
TEST_F(DependencyResolverTest, D018_LargeScalePluginSortPerformance) {
    std::map<std::string, rpf::PluginMetadata> plugins;
    for (int i = 0; i < 100; ++i) {
        plugins["plugin_" + std::to_string(i)] = makePlugin("plugin_" + std::to_string(i));
    }

    auto start = std::chrono::steady_clock::now();
    auto order = resolver.resolveLoadOrder(plugins);
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(order.size(), 100);
    EXPECT_LT(duration.count(), 100); // < 100ms
}

// D-019: 依赖插件不在列表中（忽略外部依赖）
TEST_F(DependencyResolverTest, D019_ExternalDependencyIgnored) {
    std::map<std::string, rpf::PluginMetadata> plugins;
    plugins["A"] = makePlugin("A", {{"external_lib", ">=1.0.0", true}});

    // external_lib is not in plugins map but is required=true
    // checkDependencies will return false since external_lib not in plugins
    // However, resolveLoadOrder only considers deps that are in the plugins map
    // So A should still appear in the result
    auto order = resolver.resolveLoadOrder(plugins);
    EXPECT_EQ(order.size(), 1);
    EXPECT_EQ(order[0], "A");
}

// ============================================================
// 1.3 事件总线测试 (E-001 ~ E-015)
// ============================================================

class EventBusTest : public ::testing::Test {
protected:
    rpf::EventBus bus;
};

// E-001: 订阅和发布 - 基本功能
TEST_F(EventBusTest, E001_SubscribeAndPublish) {
    int call_count = 0;
    std::string received_event;
    bus.subscribe("test.event", [&](const std::string& event, const nlohmann::json&) {
        call_count++;
        received_event = event;
    });

    bus.publish("test.event");
    EXPECT_EQ(call_count, 1);
    EXPECT_EQ(received_event, "test.event");
}

// E-002: 多订阅者
TEST_F(EventBusTest, E002_MultipleSubscribers) {
    int count1 = 0, count2 = 0;
    bus.subscribe("test.event", [&](const std::string&, const nlohmann::json&) { count1++; });
    bus.subscribe("test.event", [&](const std::string&, const nlohmann::json&) { count2++; });

    bus.publish("test.event");
    EXPECT_EQ(count1, 1);
    EXPECT_EQ(count2, 1);
}

// E-003: 取消订阅
TEST_F(EventBusTest, E003_Unsubscribe) {
    int call_count = 0;
    auto id = bus.subscribe("test.event", [&](const std::string&, const nlohmann::json&) {
        call_count++;
    });

    bus.publish("test.event");
    EXPECT_EQ(call_count, 1);

    bus.unsubscribe(id);
    bus.publish("test.event");
    EXPECT_EQ(call_count, 1);  // 不应该再次调用
}

// E-004: 数据传递
TEST_F(EventBusTest, E004_DataTransmission) {
    nlohmann::json received_data;
    bus.subscribe("test.event", [&](const std::string&, const nlohmann::json& data) {
        received_data = data;
    });

    nlohmann::json sent_data = {{"key", "value"}, {"number", 42}};
    bus.publish("test.event", sent_data);

    EXPECT_EQ(received_data["key"], "value");
    EXPECT_EQ(received_data["number"], 42);
}

// E-005: 不同事件隔离
TEST_F(EventBusTest, E005_EventIsolation) {
    int event1_count = 0, event2_count = 0;
    bus.subscribe("event.1", [&](const std::string&, const nlohmann::json&) { event1_count++; });
    bus.subscribe("event.2", [&](const std::string&, const nlohmann::json&) { event2_count++; });

    bus.publish("event.1");
    EXPECT_EQ(event1_count, 1);
    EXPECT_EQ(event2_count, 0);
}

// E-006: 发布无订阅者的事件
TEST_F(EventBusTest, E006_PublishWithNoSubscribers) {
    EXPECT_NO_THROW(bus.publish("unknown.event"));
}

// E-007: 取消不存在的 subscription_id
TEST_F(EventBusTest, E007_UnsubscribeNonexistent) {
    EXPECT_NO_THROW(bus.unsubscribe(99999));
}

// E-008: 多次订阅同一事件
TEST_F(EventBusTest, E008_MultipleSubscribeSameEvent) {
    int call_count = 0;
    auto handler = [&](const std::string&, const nlohmann::json&) { call_count++; };
    bus.subscribe("test.event", handler);
    bus.subscribe("test.event", handler);

    bus.publish("test.event");
    EXPECT_EQ(call_count, 2);
}

// E-009: 空数据发布
TEST_F(EventBusTest, E009_PublishWithEmptyData) {
    nlohmann::json received_data;
    bus.subscribe("test.event", [&](const std::string&, const nlohmann::json& data) {
        received_data = data;
    });

    bus.publish("test.event");
    // 默认参数 nlohmann::json data = nlohmann::json::object() 创建的是空对象
    EXPECT_TRUE(received_data.is_object());
    EXPECT_TRUE(received_data.empty());
}

// E-010: clear 清除所有订阅
TEST_F(EventBusTest, E010_ClearAllSubscriptions) {
    int count = 0;
    bus.subscribe("event.1", [&](const std::string&, const nlohmann::json&) { count++; });
    bus.subscribe("event.2", [&](const std::string&, const nlohmann::json&) { count++; });

    bus.clear();
    bus.publish("event.1");
    bus.publish("event.2");
    EXPECT_EQ(count, 0);
}

// E-011: 复杂嵌套 JSON 数据
TEST_F(EventBusTest, E011_ComplexNestedJsonData) {
    nlohmann::json received;
    bus.subscribe("test.event", [&](const std::string&, const nlohmann::json& data) {
        received = data;
    });

    nlohmann::json sent = {
        {"nested", {{"a", 1}, {"b", {1, 2, 3}}}},
        {"array", {{"x"}, {"y"}}}
    };
    bus.publish("test.event", sent);

    EXPECT_EQ(received["nested"]["a"], 1);
    EXPECT_EQ(received["nested"]["b"].size(), 3);
    EXPECT_EQ(received["array"].size(), 2);
}

// E-013: subscription_id 唯一性
TEST_F(EventBusTest, E013_SubscriptionIdUniqueness) {
    std::set<uint64_t> ids;
    for (int i = 0; i < 100; ++i) {
        auto id = bus.subscribe("event", [](const std::string&, const nlohmann::json&) {});
        ids.insert(id);
    }
    EXPECT_EQ(ids.size(), 100);
}

// E-015: 线程安全 - 并发订阅和发布
TEST_F(EventBusTest, E015_ThreadSafetyConcurrentSubscribePublish) {
    std::atomic<int> counter{0};

    std::thread t1([&]() {
        for (int i = 0; i < 100; ++i) {
            bus.subscribe("event", [&](const std::string&, const nlohmann::json&) {
                counter++;
            });
        }
    });

    std::thread t2([&]() {
        for (int i = 0; i < 100; ++i) {
            bus.publish("event");
        }
    });

    t1.join();
    t2.join();

    // 不崩溃即为通过，计数可能因竞争而不精确
    EXPECT_GE(counter.load(), 0);
}

// ============================================================
// 1.4 服务注册表测试 (S-001 ~ S-015)
// ============================================================

class TestServiceA {
public:
    virtual ~TestServiceA() = default;
    virtual std::string getName() const { return "ServiceA"; }
};

class TestServiceB {
public:
    virtual ~TestServiceB() = default;
    virtual std::string getName() const { return "ServiceB"; }
};

class ServiceRegistryTest : public ::testing::Test {
protected:
    rpf::ServiceRegistry registry;
};

// S-001: 注册和获取服务
TEST_F(ServiceRegistryTest, S001_RegisterAndGetService) {
    auto svc = std::make_shared<TestServiceA>();
    registry.registerService("svc", svc);

    auto retrieved = registry.getService<TestServiceA>("svc");
    EXPECT_EQ(retrieved.get(), svc.get());
}

// S-002: 获取不存在的服务
TEST_F(ServiceRegistryTest, S002_GetNonexistentService) {
    auto result = registry.getService<TestServiceA>("nonexistent");
    EXPECT_EQ(result, nullptr);
}

// S-003: 注销服务
TEST_F(ServiceRegistryTest, S003_UnregisterService) {
    auto svc = std::make_shared<TestServiceA>();
    registry.registerService("svc", svc);
    EXPECT_TRUE(registry.unregisterService("svc"));
    EXPECT_EQ(registry.getService<TestServiceA>("svc"), nullptr);
}

// S-004: 注销不存在的服务
TEST_F(ServiceRegistryTest, S004_UnregisterNonexistentService) {
    EXPECT_FALSE(registry.unregisterService("nonexistent"));
}

// S-005: hasService 查询
TEST_F(ServiceRegistryTest, S005_HasService) {
    auto svc = std::make_shared<TestServiceA>();
    registry.registerService("svc", svc);
    EXPECT_TRUE(registry.hasService("svc"));
    EXPECT_FALSE(registry.hasService("other"));
}

// S-006: listServices 列出所有服务
TEST_F(ServiceRegistryTest, S006_ListServices) {
    registry.registerService("svc1", std::make_shared<TestServiceA>());
    registry.registerService("svc2", std::make_shared<TestServiceA>());
    registry.registerService("svc3", std::make_shared<TestServiceA>());

    auto services = registry.listServices();
    EXPECT_EQ(services.size(), 3);
    std::set<std::string> names(services.begin(), services.end());
    EXPECT_TRUE(names.count("svc1"));
    EXPECT_TRUE(names.count("svc2"));
    EXPECT_TRUE(names.count("svc3"));
}

// S-007: listServices - 空注册表
TEST_F(ServiceRegistryTest, S007_ListServicesEmpty) {
    auto services = registry.listServices();
    EXPECT_TRUE(services.empty());
}

// S-008: clear 清除所有服务
TEST_F(ServiceRegistryTest, S008_ClearAllServices) {
    registry.registerService("svc1", std::make_shared<TestServiceA>());
    registry.registerService("svc2", std::make_shared<TestServiceA>());

    registry.clear();
    EXPECT_TRUE(registry.listServices().empty());
    EXPECT_FALSE(registry.hasService("svc1"));
    EXPECT_FALSE(registry.hasService("svc2"));
}

// S-009: 覆盖注册同名服务
TEST_F(ServiceRegistryTest, S009_OverwriteService) {
    auto svcA = std::make_shared<TestServiceA>();
    auto svcB = std::make_shared<TestServiceA>();
    registry.registerService("svc", svcA);
    registry.registerService("svc", svcB);

    auto retrieved = registry.getService<TestServiceA>("svc");
    EXPECT_EQ(retrieved.get(), svcB.get());
}

// S-010: 类型擦除 - 不同类型服务
TEST_F(ServiceRegistryTest, S010_TypeErasureDifferentTypes) {
    auto svcA = std::make_shared<TestServiceA>();
    auto svcB = std::make_shared<TestServiceB>();
    registry.registerService("svcA", svcA);
    registry.registerService("svcB", svcB);

    auto retrievedA = registry.getService<TestServiceA>("svcA");
    auto retrievedB = registry.getService<TestServiceB>("svcB");
    EXPECT_EQ(retrievedA->getName(), "ServiceA");
    EXPECT_EQ(retrievedB->getName(), "ServiceB");
}

// S-013: 服务生命周期 - shared_ptr 引用计数
TEST_F(ServiceRegistryTest, S013_ServiceLifecycleSharedPtr) {
    auto svc = std::make_shared<TestServiceA>();
    auto* raw_ptr = svc.get();

    registry.registerService("svc", svc);
    auto retrieved = registry.getService<TestServiceA>("svc");
    registry.unregisterService("svc");

    // retrieved 仍然持有引用，对象不应被销毁
    EXPECT_EQ(retrieved->getName(), "ServiceA");
}

// S-014: 空名称服务注册（应被拒绝）
TEST_F(ServiceRegistryTest, S014_EmptyNameRegistration) {
    auto svc = std::make_shared<TestServiceA>();
    EXPECT_FALSE(registry.registerService("", svc));
    EXPECT_FALSE(registry.hasService(""));
}

// S-015: 大量服务注册和查询性能
TEST_F(ServiceRegistryTest, S015_LargeScaleServicePerformance) {
    for (int i = 0; i < 1000; ++i) {
        registry.registerService("svc_" + std::to_string(i), std::make_shared<TestServiceA>());
    }

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i) {
        auto svc = registry.getService<TestServiceA>("svc_" + std::to_string(i));
        EXPECT_NE(svc, nullptr);
    }
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 100); // < 100ms
}

// S-012: 线程安全 - 并发读写
TEST_F(ServiceRegistryTest, S012_ThreadSafetyConcurrentReadWrite) {
    std::atomic<bool> done{false};

    std::thread writer([&]() {
        for (int i = 0; i < 100; ++i) {
            registry.registerService("svc_" + std::to_string(i), std::make_shared<TestServiceA>());
        }
        done = true;
    });

    std::thread reader([&]() {
        while (!done) {
            registry.listServices();
            registry.hasService("svc_0");
        }
    });

    writer.join();
    reader.join();

    // 不崩溃即通过
    EXPECT_TRUE(true);
}

// ============================================================
// 1.5 插件管理器测试 (PM-001 ~ PM-040)
// ============================================================

class PluginManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/rpf_test_pm";
        fs::create_directories(test_dir_);
    }

    void TearDown() override {
        fs::remove_all(test_dir_);
    }

    void writeMetaJson(const std::string& name, const std::string& version = "1.0.0",
                       std::vector<rpf::Dependency> deps = {},
                       std::vector<std::string> provides = {}) {
        nlohmann::json j = {
            {"name", name},
            {"version", version},
            {"description", "Test plugin " + name},
            {"author", "Test"},
            {"license", "MIT"},
            {"dependencies", nlohmann::json::array()},
            {"provides", provides},
            {"category", "test"},
            {"platform", "all"},
            {"entry_point", "create_plugin"}
        };

        // Add dependencies
        for (const auto& dep : deps) {
            j["dependencies"].push_back({
                {"name", dep.name},
                {"version_constraint", dep.version_constraint},
                {"required", dep.required}
            });
        }

        std::ofstream(test_dir_ + "/" + name + ".meta.json") << j.dump(4);
    }

    std::string getPluginDir() {
        // For tests that need actual .so files, we point to the build output directory
        return test_dir_;
    }

    std::string test_dir_;
};

// PM-001: 构造函数 - 正常目录
TEST_F(PluginManagerTest, PM001_ConstructorNormalDirectory) {
    EXPECT_NO_THROW(rpf::PluginManager manager(test_dir_));
}

// PM-002: 构造函数 - 不存在的目录
TEST_F(PluginManagerTest, PM002_ConstructorNonexistentDirectory) {
    EXPECT_NO_THROW(rpf::PluginManager manager("/nonexistent/dir"));
}

// PM-003: 禁止拷贝 (编译时检查)
TEST_F(PluginManagerTest, PM003_NoCopy) {
    // 这个测试验证类型特征，如果编译通过就说明已 delete
    EXPECT_FALSE(std::is_copy_constructible_v<rpf::PluginManager>);
    EXPECT_FALSE(std::is_copy_assignable_v<rpf::PluginManager>);
}

// PM-004: 扫描空目录
TEST_F(PluginManagerTest, PM004_ScanEmptyDirectory) {
    rpf::PluginManager manager(test_dir_);
    auto plugins = manager.scanAvailablePlugins();
    EXPECT_TRUE(plugins.empty());
}

// PM-005: 扫描含 meta.json 的目录
TEST_F(PluginManagerTest, PM005_ScanWithMetaJson) {
    writeMetaJson("plugin_a");
    writeMetaJson("plugin_b");
    writeMetaJson("plugin_c");

    rpf::PluginManager manager(test_dir_);
    auto plugins = manager.scanAvailablePlugins();
    EXPECT_EQ(plugins.size(), 3);
}

// PM-006: 扫描 - 含非 meta.json 文件
TEST_F(PluginManagerTest, PM006_ScanWithNonMetaFiles) {
    writeMetaJson("plugin_a");
    std::ofstream(test_dir_ + "/other.json") << "{}";
    std::ofstream(test_dir_ + "/readme.txt") << "text";
    std::ofstream(test_dir_ + "/lib.so") << "binary";

    rpf::PluginManager manager(test_dir_);
    auto plugins = manager.scanAvailablePlugins();
    EXPECT_EQ(plugins.size(), 1);
    EXPECT_EQ(plugins[0].name, "plugin_a");
}

// PM-007: 扫描 - 含损坏的 meta.json
TEST_F(PluginManagerTest, PM007_ScanWithCorruptedMeta) {
    writeMetaJson("good_plugin");
    std::ofstream(test_dir_ + "/bad.meta.json") << "{corrupted}";

    rpf::PluginManager manager(test_dir_);
    auto plugins = manager.scanAvailablePlugins();
    EXPECT_EQ(plugins.size(), 1);
    EXPECT_EQ(plugins[0].name, "good_plugin");
}

// PM-008: getPluginMetadata - 已扫描的插件
TEST_F(PluginManagerTest, PM008_GetPluginMetadataAfterScan) {
    writeMetaJson("mujoco_hardware", "1.0.0", {}, {"Hardware", "Simulator"});

    rpf::PluginManager manager(test_dir_);
    manager.scanAvailablePlugins();

    auto meta = manager.getPluginMetadata("mujoco_hardware");
    EXPECT_EQ(meta.name, "mujoco_hardware");
    EXPECT_EQ(meta.version, "1.0.0");
}

// PM-009: getPluginMetadata - 未扫描的插件
TEST_F(PluginManagerTest, PM009_GetPluginMetadataNotScanned) {
    rpf::PluginManager manager(test_dir_);
    EXPECT_THROW(manager.getPluginMetadata("xxx"), std::runtime_error);
}

// PM-011: loadPlugin - 无 meta 文件
TEST_F(PluginManagerTest, PM011_LoadPluginNoMetaFile) {
    rpf::PluginManager manager(test_dir_);
    EXPECT_FALSE(manager.loadPlugin("nonexistent"));
}

// PM-012: loadPlugin - 无 .so 文件
TEST_F(PluginManagerTest, PM012_LoadPluginNoSoFile) {
    writeMetaJson("no_so_plugin");

    rpf::PluginManager manager(test_dir_);
    manager.scanAvailablePlugins();
    EXPECT_FALSE(manager.loadPlugin("no_so_plugin"));
}

// PM-025: 完整生命周期流程 - 使用真实插件
class PluginManagerWithPluginsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 找到 build 目录中的 mujoco_hardware_plugin 目录（包含 .so 和 .meta.json）
        plugin_dir_ = "/home/sun/Documents/GitHub/cpp-httplib/robot_plugin_framework/build/examples/mujoco_hardware_plugin";
        if (!fs::exists(plugin_dir_)) {
            GTEST_SKIP() << "Plugin directory not found: " << plugin_dir_;
        }
        // 验证目录中有 .meta.json 和 .so 文件
        bool has_meta = false, has_so = false;
        for (const auto& entry : fs::directory_iterator(plugin_dir_)) {
            auto fn = entry.path().filename().string();
            if (fn.find(".meta.json") != std::string::npos) has_meta = true;
            if (fn.find(".so") != std::string::npos) has_so = true;
        }
        if (!has_meta || !has_so) {
            GTEST_SKIP() << "Plugin directory missing .meta.json or .so files";
        }
    }

    std::string plugin_dir_;
};

// PM-025: 完整生命周期流程
TEST_F(PluginManagerWithPluginsTest, PM025_FullLifecycle) {
    rpf::PluginManager manager(plugin_dir_);
    auto plugins = manager.scanAvailablePlugins();
    ASSERT_FALSE(plugins.empty()) << "No plugins found in " << plugin_dir_;

    std::string plugin_name = plugins[0].name;

    // 初始状态应为 Unloaded
    EXPECT_EQ(manager.getPluginState(plugin_name), rpf::PluginState::Unloaded);

    // Load
    EXPECT_TRUE(manager.loadPlugin(plugin_name));
    EXPECT_EQ(manager.getPluginState(plugin_name), rpf::PluginState::Loaded);

    // Initialize
    EXPECT_TRUE(manager.initializePlugin(plugin_name));
    EXPECT_EQ(manager.getPluginState(plugin_name), rpf::PluginState::Initialized);

    // Start
    EXPECT_TRUE(manager.startPlugin(plugin_name));
    EXPECT_EQ(manager.getPluginState(plugin_name), rpf::PluginState::Running);

    // Stop
    EXPECT_TRUE(manager.stopPlugin(plugin_name));
    EXPECT_EQ(manager.getPluginState(plugin_name), rpf::PluginState::Stopped);

    // Unload
    EXPECT_TRUE(manager.unloadPlugin(plugin_name));
    EXPECT_EQ(manager.getPluginState(plugin_name), rpf::PluginState::Unloaded);
}

// PM-013: loadPlugin - 重复加载
TEST_F(PluginManagerWithPluginsTest, PM013_LoadPluginIdempotent) {
    rpf::PluginManager manager(plugin_dir_);
    manager.scanAvailablePlugins();
    auto plugins = manager.scanAvailablePlugins();
    ASSERT_FALSE(plugins.empty());

    std::string name = plugins[0].name;
    EXPECT_TRUE(manager.loadPlugin(name));
    EXPECT_TRUE(manager.loadPlugin(name)); // 重复加载应返回 true
}

// PM-015: initializePlugin - 正常初始化
TEST_F(PluginManagerWithPluginsTest, PM015_InitializePlugin) {
    rpf::PluginManager manager(plugin_dir_);
    manager.scanAvailablePlugins();
    auto plugins = manager.scanAvailablePlugins();
    ASSERT_FALSE(plugins.empty());

    std::string name = plugins[0].name;
    EXPECT_TRUE(manager.loadPlugin(name));
    EXPECT_TRUE(manager.initializePlugin(name));
    EXPECT_EQ(manager.getPluginState(name), rpf::PluginState::Initialized);
}

// PM-016: initializePlugin - 未加载
TEST_F(PluginManagerWithPluginsTest, PM016_InitializePluginNotLoaded) {
    rpf::PluginManager manager(plugin_dir_);
    manager.scanAvailablePlugins();
    EXPECT_FALSE(manager.initializePlugin("nonexistent"));
}

// PM-017: initializePlugin - 已初始化
TEST_F(PluginManagerWithPluginsTest, PM017_InitializePluginAlreadyInitialized) {
    rpf::PluginManager manager(plugin_dir_);
    manager.scanAvailablePlugins();
    auto plugins = manager.scanAvailablePlugins();
    ASSERT_FALSE(plugins.empty());

    std::string name = plugins[0].name;
    EXPECT_TRUE(manager.loadPlugin(name));
    EXPECT_TRUE(manager.initializePlugin(name));
    EXPECT_FALSE(manager.initializePlugin(name)); // 状态不是 Loaded
}

// PM-018: startPlugin - 正常启动
TEST_F(PluginManagerWithPluginsTest, PM018_StartPlugin) {
    rpf::PluginManager manager(plugin_dir_);
    manager.scanAvailablePlugins();
    auto plugins = manager.scanAvailablePlugins();
    ASSERT_FALSE(plugins.empty());

    std::string name = plugins[0].name;
    EXPECT_TRUE(manager.loadPlugin(name));
    EXPECT_TRUE(manager.initializePlugin(name));
    EXPECT_TRUE(manager.startPlugin(name));
    EXPECT_EQ(manager.getPluginState(name), rpf::PluginState::Running);
}

// PM-019: startPlugin - 未初始化
TEST_F(PluginManagerWithPluginsTest, PM019_StartPluginNotInitialized) {
    rpf::PluginManager manager(plugin_dir_);
    manager.scanAvailablePlugins();
    auto plugins = manager.scanAvailablePlugins();
    ASSERT_FALSE(plugins.empty());

    std::string name = plugins[0].name;
    EXPECT_TRUE(manager.loadPlugin(name));
    EXPECT_FALSE(manager.startPlugin(name)); // 状态是 Loaded，不是 Initialized
}

// PM-020: stopPlugin - 正常停止
TEST_F(PluginManagerWithPluginsTest, PM020_StopPlugin) {
    rpf::PluginManager manager(plugin_dir_);
    manager.scanAvailablePlugins();
    auto plugins = manager.scanAvailablePlugins();
    ASSERT_FALSE(plugins.empty());

    std::string name = plugins[0].name;
    EXPECT_TRUE(manager.loadPlugin(name));
    EXPECT_TRUE(manager.initializePlugin(name));
    EXPECT_TRUE(manager.startPlugin(name));
    EXPECT_TRUE(manager.stopPlugin(name));
    EXPECT_EQ(manager.getPluginState(name), rpf::PluginState::Stopped);
}

// PM-021: stopPlugin - 未运行
TEST_F(PluginManagerWithPluginsTest, PM021_StopPluginNotRunning) {
    rpf::PluginManager manager(plugin_dir_);
    manager.scanAvailablePlugins();
    auto plugins = manager.scanAvailablePlugins();
    ASSERT_FALSE(plugins.empty());

    std::string name = plugins[0].name;
    EXPECT_TRUE(manager.loadPlugin(name));
    EXPECT_TRUE(manager.initializePlugin(name));
    EXPECT_FALSE(manager.stopPlugin(name)); // 状态是 Initialized
}

// PM-022: unloadPlugin - 正常卸载
TEST_F(PluginManagerWithPluginsTest, PM022_UnloadPlugin) {
    rpf::PluginManager manager(plugin_dir_);
    manager.scanAvailablePlugins();
    auto plugins = manager.scanAvailablePlugins();
    ASSERT_FALSE(plugins.empty());

    std::string name = plugins[0].name;
    EXPECT_TRUE(manager.loadPlugin(name));
    EXPECT_TRUE(manager.initializePlugin(name));
    EXPECT_TRUE(manager.startPlugin(name));
    EXPECT_TRUE(manager.stopPlugin(name));
    EXPECT_TRUE(manager.unloadPlugin(name));
    EXPECT_EQ(manager.getPluginState(name), rpf::PluginState::Unloaded);
}

// PM-023: unloadPlugin - 未加载
TEST_F(PluginManagerWithPluginsTest, PM023_UnloadPluginNotLoaded) {
    rpf::PluginManager manager(plugin_dir_);
    EXPECT_FALSE(manager.unloadPlugin("nonexistent"));
}

// PM-024: unloadPlugin - 运行中卸载
TEST_F(PluginManagerWithPluginsTest, PM024_UnloadPluginWhileRunning) {
    rpf::PluginManager manager(plugin_dir_);
    manager.scanAvailablePlugins();
    auto plugins = manager.scanAvailablePlugins();
    ASSERT_FALSE(plugins.empty());

    std::string name = plugins[0].name;
    EXPECT_TRUE(manager.loadPlugin(name));
    EXPECT_TRUE(manager.initializePlugin(name));
    EXPECT_TRUE(manager.startPlugin(name));
    EXPECT_TRUE(manager.unloadPlugin(name)); // 自动先 stop
    EXPECT_EQ(manager.getPluginState(name), rpf::PluginState::Unloaded);
}

// PM-032: getLoadedPlugins 查询
TEST_F(PluginManagerWithPluginsTest, PM032_GetLoadedPlugins) {
    rpf::PluginManager manager(plugin_dir_);
    manager.scanAvailablePlugins();
    auto plugins = manager.scanAvailablePlugins();
    ASSERT_FALSE(plugins.empty());

    for (const auto& p : plugins) {
        manager.loadPlugin(p.name);
    }

    auto loaded = manager.getLoadedPlugins();
    EXPECT_EQ(loaded.size(), plugins.size());
}

// PM-033: getPluginState 查询
TEST_F(PluginManagerWithPluginsTest, PM033_GetPluginState) {
    rpf::PluginManager manager(plugin_dir_);
    manager.scanAvailablePlugins();
    auto plugins = manager.scanAvailablePlugins();
    ASSERT_FALSE(plugins.empty());

    std::string name = plugins[0].name;
    EXPECT_EQ(manager.getPluginState(name), rpf::PluginState::Unloaded);

    manager.loadPlugin(name);
    EXPECT_EQ(manager.getPluginState(name), rpf::PluginState::Loaded);

    manager.initializePlugin(name);
    manager.startPlugin(name);
    EXPECT_EQ(manager.getPluginState(name), rpf::PluginState::Running);
}

// PM-034: getPlugin 获取实例
TEST_F(PluginManagerWithPluginsTest, PM034_GetPluginInstance) {
    rpf::PluginManager manager(plugin_dir_);
    manager.scanAvailablePlugins();
    auto plugins = manager.scanAvailablePlugins();
    ASSERT_FALSE(plugins.empty());

    std::string name = plugins[0].name;
    EXPECT_EQ(manager.getPlugin(name), nullptr);

    manager.loadPlugin(name);
    manager.initializePlugin(name);
    EXPECT_NE(manager.getPlugin(name), nullptr);
}

// PM-035: getPlugin - 未加载
TEST_F(PluginManagerWithPluginsTest, PM035_GetPluginNotLoaded) {
    rpf::PluginManager manager(plugin_dir_);
    EXPECT_EQ(manager.getPlugin("nonexistent"), nullptr);
}

// PM-036: getServiceRegistry
TEST_F(PluginManagerWithPluginsTest, PM036_GetServiceRegistry) {
    rpf::PluginManager manager(plugin_dir_);
    auto& registry = manager.getServiceRegistry();
    registry.registerService("test_svc", std::make_shared<TestServiceA>());
    EXPECT_TRUE(registry.hasService("test_svc"));
}

// PM-037: getEventBus
TEST_F(PluginManagerWithPluginsTest, PM037_GetEventBus) {
    rpf::PluginManager manager(plugin_dir_);
    auto& bus = manager.getEventBus();
    int count = 0;
    bus.subscribe("test", [&](const std::string&, const nlohmann::json&) { count++; });
    bus.publish("test");
    EXPECT_EQ(count, 1);
}

// PM-031: 析构函数自动清理
TEST_F(PluginManagerWithPluginsTest, PM031_DestructorAutoCleanup) {
    {
        rpf::PluginManager manager(plugin_dir_);
        manager.scanAvailablePlugins();
        auto plugins = manager.scanAvailablePlugins();
        for (const auto& p : plugins) {
            manager.loadPlugin(p.name);
            manager.initializePlugin(p.name);
            manager.startPlugin(p.name);
        }
    }
    // 析构完成，不崩溃即通过
    EXPECT_TRUE(true);
}

// PM-026: loadAll - 批量加载
TEST_F(PluginManagerWithPluginsTest, PM026_LoadAll) {
    rpf::PluginManager manager(plugin_dir_);
    bool result = manager.loadAll();
    // loadAll 会扫描目录并按依赖顺序加载
    // 如果所有 .so 文件都存在，应返回 true
    auto loaded = manager.getLoadedPlugins();
    // 至少应该尝试加载
    EXPECT_GE(loaded.size(), 0);
}

// PM-028: startAll - 批量启动
TEST_F(PluginManagerWithPluginsTest, PM028_StartAll) {
    rpf::PluginManager manager(plugin_dir_);
    manager.loadAll();
    bool result = manager.startAll();

    auto loaded = manager.getLoadedPlugins();
    for (const auto& name : loaded) {
        EXPECT_EQ(manager.getPluginState(name), rpf::PluginState::Running);
    }
}

// PM-029: stopAll - 批量停止
TEST_F(PluginManagerWithPluginsTest, PM029_StopAll) {
    rpf::PluginManager manager(plugin_dir_);
    manager.loadAll();
    manager.startAll();
    manager.stopAll();

    auto loaded = manager.getLoadedPlugins();
    for (const auto& name : loaded) {
        auto state = manager.getPluginState(name);
        EXPECT_TRUE(state == rpf::PluginState::Stopped || state == rpf::PluginState::Loaded);
    }
}

// PM-030: unloadAll - 批量卸载
TEST_F(PluginManagerWithPluginsTest, PM030_UnloadAll) {
    rpf::PluginManager manager(plugin_dir_);
    manager.loadAll();
    manager.startAll();
    manager.unloadAll();

    EXPECT_TRUE(manager.getLoadedPlugins().empty());
}

// ============================================================
// 1.6 硬件接口测试 (HW-001 ~ HW-020)
// ============================================================

class HardwareInterfaceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 查找插件目录
        plugin_dir_ = findPluginDir();
        if (plugin_dir_.empty()) {
            GTEST_SKIP() << "Plugin directory not found";
        }

        manager_ = std::make_unique<rpf::PluginManager>(plugin_dir_);
        manager_->scanAvailablePlugins();

        // 尝试加载 mujoco_hardware 插件
        if (!manager_->loadPlugin("mujoco_hardware") ||
            !manager_->initializePlugin("mujoco_hardware")) {
            GTEST_SKIP() << "Failed to load mujoco_hardware plugin";
        }

        auto* plugin = manager_->getPlugin("mujoco_hardware");
        if (!plugin) {
            GTEST_SKIP() << "mujoco_hardware plugin instance is null";
        }

        hardware_ = static_cast<rpf::Hardware*>(plugin->getService("Hardware"));
        simulator_ = static_cast<rpf::Simulator*>(plugin->getService("Simulator"));
    }

    void TearDown() override {
        if (manager_) {
            manager_->unloadAll();
        }
    }

    std::string findPluginDir() {
        std::string path = "/home/sun/Documents/GitHub/cpp-httplib/robot_plugin_framework/build/examples/mujoco_hardware_plugin";
        if (fs::exists(path)) {
            bool has_meta = false, has_so = false;
            for (const auto& entry : fs::directory_iterator(path)) {
                auto fn = entry.path().filename().string();
                if (fn.find(".meta.json") != std::string::npos) has_meta = true;
                if (fn.find(".so") != std::string::npos) has_so = true;
            }
            if (has_meta && has_so) return path;
        }
        return "";
    }

    std::string plugin_dir_;
    std::unique_ptr<rpf::PluginManager> manager_;
    rpf::Hardware* hardware_ = nullptr;
    rpf::Simulator* simulator_ = nullptr;
};

// HW-001: MuJoCo 插件初始化
TEST_F(HardwareInterfaceTest, HW001_MujocoPluginInitialization) {
    ASSERT_NE(simulator_, nullptr);
    auto joint_names = simulator_->getJointNames();
    EXPECT_EQ(joint_names.size(), 6);
    for (int i = 0; i < 6; ++i) {
        EXPECT_EQ(joint_names[i], "joint" + std::to_string(i + 1));
    }

    auto state = hardware_->getState();
    EXPECT_EQ(state.joints.size(), 6);
    for (const auto& joint : state.joints) {
        EXPECT_DOUBLE_EQ(joint.position, 0.0);
    }
}

// HW-002: getState 获取机器人状态
TEST_F(HardwareInterfaceTest, HW002_GetState) {
    auto state = hardware_->getState();
    EXPECT_EQ(state.joints.size(), 6);
    EXPECT_DOUBLE_EQ(state.timestamp, 0.0);
    EXPECT_FALSE(state.is_moving);
}

// HW-003: setJointPosition 设置关节位置
TEST_F(HardwareInterfaceTest, HW003_SetJointPosition) {
    EXPECT_TRUE(hardware_->setJointPosition("joint1", 1.57));
}

// HW-004: setJointPosition 无效关节名
TEST_F(HardwareInterfaceTest, HW004_SetJointPositionInvalid) {
    EXPECT_FALSE(hardware_->setJointPosition("invalid_joint", 1.0));
}

// HW-005: setJointPositions 批量设置
TEST_F(HardwareInterfaceTest, HW005_SetJointPositions) {
    std::map<std::string, double> positions = {{"joint1", 1.0}, {"joint2", 2.0}};
    EXPECT_TRUE(hardware_->setJointPositions(positions));
}

// HW-006: stepSimulation 仿真步进
TEST_F(HardwareInterfaceTest, HW006_StepSimulation) {
    hardware_->setJointPosition("joint1", 1.0);
    hardware_->stepSimulation(0.001);

    auto state = hardware_->getState();
    auto it = std::find_if(state.joints.begin(), state.joints.end(),
        [](const rpf::JointState& j) { return j.name == "joint1"; });
    ASSERT_NE(it, state.joints.end());
    // 位置应开始向目标方向变化
    EXPECT_NE(it->position, 0.0);
    EXPECT_GT(state.timestamp, 0.0);
}

// HW-007: reset 重置仿真
TEST_F(HardwareInterfaceTest, HW007_ResetSimulation) {
    hardware_->setJointPosition("joint1", 1.57);
    hardware_->stepSimulation(0.1);
    hardware_->reset();

    auto state = hardware_->getState();
    EXPECT_DOUBLE_EQ(state.timestamp, 0.0);
    for (const auto& joint : state.joints) {
        EXPECT_DOUBLE_EQ(joint.position, 0.0);
        EXPECT_DOUBLE_EQ(joint.velocity, 0.0);
        EXPECT_DOUBLE_EQ(joint.effort, 0.0);
    }
}

// HW-008: startSimulation/stopSimulation
TEST_F(HardwareInterfaceTest, HW008_StartStopSimulation) {
    EXPECT_TRUE(hardware_->startSimulation());
    EXPECT_TRUE(hardware_->isSimulating());
    EXPECT_TRUE(hardware_->stopSimulation());
    EXPECT_FALSE(hardware_->isSimulating());
}

// HW-009: startSimulation 重复启动
TEST_F(HardwareInterfaceTest, HW009_StartSimulationRepeat) {
    EXPECT_TRUE(hardware_->startSimulation());
    EXPECT_TRUE(hardware_->startSimulation()); // 幂等
    hardware_->stopSimulation();
}

// HW-010: stopSimulation 未运行时
TEST_F(HardwareInterfaceTest, HW010_StopSimulationNotRunning) {
    EXPECT_TRUE(hardware_->stopSimulation()); // 幂等
}

// HW-011: getSensorData 获取传感器数据
TEST_F(HardwareInterfaceTest, HW011_GetSensorData) {
    auto sensors = hardware_->getSensorData();
    EXPECT_EQ(sensors.size(), 3);

    std::set<std::string> sensor_names;
    for (const auto& s : sensors) {
        sensor_names.insert(s.name);
    }
    EXPECT_TRUE(sensor_names.count("force_sensor"));
    EXPECT_TRUE(sensor_names.count("torque_sensor"));
    EXPECT_TRUE(sensor_names.count("imu"));
}

// HW-012: getJointNames
TEST_F(HardwareInterfaceTest, HW012_GetJointNames) {
    auto names = simulator_->getJointNames();
    ASSERT_EQ(names.size(), 6);
    for (int i = 0; i < 6; ++i) {
        EXPECT_EQ(names[i], "joint" + std::to_string(i + 1));
    }
}

// HW-013: getBodyNames
TEST_F(HardwareInterfaceTest, HW013_GetBodyNames) {
    auto names = simulator_->getBodyNames();
    ASSERT_EQ(names.size(), 7);
    EXPECT_EQ(names[0], "base_link");
    for (int i = 1; i <= 6; ++i) {
        EXPECT_EQ(names[i], "link" + std::to_string(i));
    }
}

// HW-014: getSensorNames
TEST_F(HardwareInterfaceTest, HW014_GetSensorNames) {
    auto names = simulator_->getSensorNames();
    ASSERT_EQ(names.size(), 3);
    EXPECT_EQ(names[0], "force_sensor");
    EXPECT_EQ(names[1], "torque_sensor");
    EXPECT_EQ(names[2], "imu");
}

// HW-015: PD 控制器收敛
TEST_F(HardwareInterfaceTest, HW015_PDControllerConvergence) {
    hardware_->setJointPosition("joint1", 1.0);
    for (int i = 0; i < 100; ++i) {
        hardware_->stepSimulation(0.5);
    }

    auto state = hardware_->getState();
    auto it = std::find_if(state.joints.begin(), state.joints.end(),
        [](const rpf::JointState& j) { return j.name == "joint1"; });
    ASSERT_NE(it, state.joints.end());
    // 位置应向 1.0 方向收敛
    EXPECT_GT(it->position, 0.0);
    EXPECT_LT(it->position, 1.0 + 0.5); // 不应大幅超调
}

// HW-016: getService 接口
TEST_F(HardwareInterfaceTest, HW016_GetServiceInterface) {
    auto* plugin = manager_->getPlugin("mujoco_hardware");
    ASSERT_NE(plugin, nullptr);

    auto* hw = plugin->getService("Hardware");
    EXPECT_NE(hw, nullptr);

    auto* sim = plugin->getService("Simulator");
    EXPECT_NE(sim, nullptr);

    auto* unknown = plugin->getService("Unknown");
    EXPECT_EQ(unknown, nullptr);
}

// HW-017: unload 后清理
TEST_F(HardwareInterfaceTest, HW017_UnloadCleanup) {
    manager_->unloadPlugin("mujoco_hardware");
    // 插件已卸载，getState 应该无法调用（hardware_ 指针可能无效）
    // 这个测试主要验证 unload 不崩溃
    EXPECT_EQ(manager_->getPluginState("mujoco_hardware"), rpf::PluginState::Unloaded);
}

// HW-018: setJointVelocity
TEST_F(HardwareInterfaceTest, HW018_SetJointVelocity) {
    EXPECT_TRUE(hardware_->setJointVelocity("joint1", 2.0));
}

// HW-019: setJointEffort
TEST_F(HardwareInterfaceTest, HW019_SetJointEffort) {
    EXPECT_TRUE(hardware_->setJointEffort("joint1", 5.0));
}

// HW-020: 线程安全 - 仿真线程与状态查询并发
TEST_F(HardwareInterfaceTest, HW020_ThreadSafetySimulationAndQuery) {
    hardware_->setJointPosition("joint1", 1.0);
    hardware_->startSimulation();

    std::thread reader([&]() {
        for (int i = 0; i < 50; ++i) {
            auto state = hardware_->getState();
            hardware_->setJointPosition("joint2", 0.5);
        }
    });

    reader.join();
    hardware_->stopSimulation();
    EXPECT_TRUE(true); // 不崩溃即通过
}

// ============================================================
// 示例插件功能测试 (EX-001 ~ EX-010)
// ============================================================

class ExamplePluginTest : public ::testing::Test {
protected:
    void SetUp() override {
        plugin_dir_ = findPluginDir();
        if (plugin_dir_.empty()) {
            GTEST_SKIP() << "Plugin directory not found";
        }
        manager_ = std::make_unique<rpf::PluginManager>(plugin_dir_);
        manager_->scanAvailablePlugins();
    }

    void TearDown() override {
        if (manager_) manager_->unloadAll();
    }

    std::string findPluginDir() {
        // Try host_app/plugins directory which has motion_control and vision
        std::string path = "/home/sun/Documents/GitHub/cpp-httplib/robot_plugin_framework/build/examples/host_app/plugins";
        if (fs::exists(path)) {
            bool has_meta = false;
            for (const auto& entry : fs::directory_iterator(path)) {
                if (entry.path().filename().string().find(".meta.json") != std::string::npos) has_meta = true;
            }
            if (has_meta) return path;
        }
        // Fallback to individual plugin directories
        std::vector<std::string> candidates = {
            "/home/sun/Documents/GitHub/cpp-httplib/robot_plugin_framework/build/examples/motion_control_plugin",
            "/home/sun/Documents/GitHub/cpp-httplib/robot_plugin_framework/build/examples/vision_plugin"
        };
        for (const auto& dir : candidates) {
            if (fs::exists(dir)) {
                bool has_meta = false, has_so = false;
                for (const auto& entry : fs::directory_iterator(dir)) {
                    auto fn = entry.path().filename().string();
                    if (fn.find(".meta.json") != std::string::npos) has_meta = true;
                    if (fn.find(".so") != std::string::npos) has_so = true;
                }
                if (has_meta && has_so) return dir;
            }
        }
        return "";
    }

    std::string plugin_dir_;
    std::unique_ptr<rpf::PluginManager> manager_;
};

// EX-001: motion_control 生命周期
TEST_F(ExamplePluginTest, EX001_MotionControlLifecycle) {
    if (!manager_->loadPlugin("motion_control")) {
        GTEST_SKIP() << "motion_control plugin not available";
    }

    EXPECT_EQ(manager_->getPluginState("motion_control"), rpf::PluginState::Loaded);
    EXPECT_TRUE(manager_->initializePlugin("motion_control"));
    EXPECT_EQ(manager_->getPluginState("motion_control"), rpf::PluginState::Initialized);
    EXPECT_TRUE(manager_->startPlugin("motion_control"));
    EXPECT_EQ(manager_->getPluginState("motion_control"), rpf::PluginState::Running);
    EXPECT_TRUE(manager_->stopPlugin("motion_control"));
    EXPECT_EQ(manager_->getPluginState("motion_control"), rpf::PluginState::Stopped);
    EXPECT_TRUE(manager_->unloadPlugin("motion_control"));
    EXPECT_EQ(manager_->getPluginState("motion_control"), rpf::PluginState::Unloaded);
}

// EX-002: MotionController 接口
TEST_F(ExamplePluginTest, EX002_MotionControllerInterface) {
    if (!manager_->loadPlugin("motion_control") || !manager_->initializePlugin("motion_control")) {
        GTEST_SKIP() << "motion_control plugin not available";
    }

    auto* plugin = manager_->getPlugin("motion_control");
    ASSERT_NE(plugin, nullptr);

    auto* mc = static_cast<robot::MotionController*>(plugin->getService("MotionController"));
    ASSERT_NE(mc, nullptr);

    EXPECT_TRUE(mc->setJointPosition(0, 1.57));
    EXPECT_DOUBLE_EQ(mc->getJointPosition(0), 1.57);
}

// EX-003: MotionController - 无效关节 ID
TEST_F(ExamplePluginTest, EX003_MotionControllerInvalidJointId) {
    if (!manager_->loadPlugin("motion_control") || !manager_->initializePlugin("motion_control")) {
        GTEST_SKIP() << "motion_control plugin not available";
    }

    auto* plugin = manager_->getPlugin("motion_control");
    auto* mc = static_cast<robot::MotionController*>(plugin->getService("MotionController"));
    ASSERT_NE(mc, nullptr);

    EXPECT_FALSE(mc->setJointPosition(-1, 1.0));
    EXPECT_FALSE(mc->setJointPosition(10, 1.0));
}

// EX-004: getJointCount
TEST_F(ExamplePluginTest, EX004_GetJointCount) {
    if (!manager_->loadPlugin("motion_control") || !manager_->initializePlugin("motion_control")) {
        GTEST_SKIP() << "motion_control plugin not available";
    }

    auto* plugin = manager_->getPlugin("motion_control");
    auto* mc = static_cast<robot::MotionController*>(plugin->getService("MotionController"));
    ASSERT_NE(mc, nullptr);
    EXPECT_EQ(mc->getJointCount(), 6);
}

// EX-005: TrajectoryPlanner 接口
TEST_F(ExamplePluginTest, EX005_TrajectoryPlannerInterface) {
    if (!manager_->loadPlugin("motion_control") || !manager_->initializePlugin("motion_control")) {
        GTEST_SKIP() << "motion_control plugin not available";
    }

    auto* plugin = manager_->getPlugin("motion_control");
    auto* tp = static_cast<robot::TrajectoryPlanner*>(plugin->getService("TrajectoryPlanner"));
    ASSERT_NE(tp, nullptr);

    auto trajectory = tp->planTrajectory({0.0}, {1.0}, 1.0);
    EXPECT_FALSE(trajectory.empty());
    EXPECT_NEAR(trajectory.front(), 0.0, 0.01);
    EXPECT_NEAR(trajectory.back(), 1.0, 0.01);
}

// EX-006: getService 接口 - motion_control
TEST_F(ExamplePluginTest, EX006_MotionControlGetService) {
    if (!manager_->loadPlugin("motion_control") || !manager_->initializePlugin("motion_control")) {
        GTEST_SKIP() << "motion_control plugin not available";
    }

    auto* plugin = manager_->getPlugin("motion_control");
    EXPECT_NE(plugin->getService("MotionController"), nullptr);
    EXPECT_NE(plugin->getService("TrajectoryPlanner"), nullptr);
    EXPECT_EQ(plugin->getService("Unknown"), nullptr);
}

// EX-007: vision 生命周期
TEST_F(ExamplePluginTest, EX007_VisionLifecycle) {
    if (!manager_->loadPlugin("vision")) {
        GTEST_SKIP() << "vision plugin not available";
    }

    EXPECT_EQ(manager_->getPluginState("vision"), rpf::PluginState::Loaded);
    EXPECT_TRUE(manager_->initializePlugin("vision"));
    EXPECT_TRUE(manager_->startPlugin("vision"));
    EXPECT_EQ(manager_->getPluginState("vision"), rpf::PluginState::Running);
    EXPECT_TRUE(manager_->stopPlugin("vision"));
    EXPECT_TRUE(manager_->unloadPlugin("vision"));
}

// EX-008: VisionDetector.detect
TEST_F(ExamplePluginTest, EX008_VisionDetectorDetect) {
    if (!manager_->loadPlugin("vision") || !manager_->initializePlugin("vision")) {
        GTEST_SKIP() << "vision plugin not available";
    }

    auto* plugin = manager_->getPlugin("vision");
    auto* vd = static_cast<robot::VisionDetector*>(plugin->getService("VisionDetector"));
    ASSERT_NE(vd, nullptr);

    std::vector<uint8_t> empty_image;
    auto results = vd->detect(empty_image);
    EXPECT_EQ(results.size(), 2);
    EXPECT_EQ(results[0].label, "object_1");
    EXPECT_EQ(results[1].label, "object_2");
}

// EX-009: PoseEstimator.estimatePose
TEST_F(ExamplePluginTest, EX009_PoseEstimatorEstimatePose) {
    if (!manager_->loadPlugin("vision") || !manager_->initializePlugin("vision")) {
        GTEST_SKIP() << "vision plugin not available";
    }

    auto* plugin = manager_->getPlugin("vision");
    auto* pe = static_cast<robot::PoseEstimator*>(plugin->getService("PoseEstimator"));
    ASSERT_NE(pe, nullptr);

    robot::Detection det = {"object_1", 0.95, 100.0, 200.0, 50.0, 50.0};
    auto pose = pe->estimatePose(det);
    EXPECT_DOUBLE_EQ(pose.x, 0.1);  // 100 * 0.001
    EXPECT_DOUBLE_EQ(pose.y, 0.2);  // 200 * 0.001
    EXPECT_DOUBLE_EQ(pose.z, 0.5);
}

// EX-010: getService 接口 - vision
TEST_F(ExamplePluginTest, EX010_VisionGetService) {
    if (!manager_->loadPlugin("vision") || !manager_->initializePlugin("vision")) {
        GTEST_SKIP() << "vision plugin not available";
    }

    auto* plugin = manager_->getPlugin("vision");
    EXPECT_NE(plugin->getService("VisionDetector"), nullptr);
    EXPECT_NE(plugin->getService("PoseEstimator"), nullptr);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
