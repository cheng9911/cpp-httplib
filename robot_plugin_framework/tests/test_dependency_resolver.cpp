#include <gtest/gtest.h>
#include "rpf/dependency_resolver.h"

class DependencyResolverTest : public ::testing::Test {
protected:
    rpf::DependencyResolver resolver;
};

TEST_F(DependencyResolverTest, NoDependencies) {
    std::map<std::string, rpf::PluginMetadata> plugins;
    plugins["A"] = {"A", "1.0.0", "", "", "", {}, {}, "test", "all", "create_plugin"};
    plugins["B"] = {"B", "1.0.0", "", "", "", {}, {}, "test", "all", "create_plugin"};
    plugins["C"] = {"C", "1.0.0", "", "", "", {}, {}, "test", "all", "create_plugin"};

    auto order = resolver.resolveLoadOrder(plugins);
    EXPECT_EQ(order.size(), 3);
}

TEST_F(DependencyResolverTest, LinearDependencies) {
    std::map<std::string, rpf::PluginMetadata> plugins;

    rpf::PluginMetadata c;
    c.name = "C";
    c.version = "1.0.0";
    plugins["C"] = c;

    rpf::PluginMetadata b;
    b.name = "B";
    b.version = "1.0.0";
    b.dependencies = {{"C", ">=1.0.0", true}};
    plugins["B"] = b;

    rpf::PluginMetadata a;
    a.name = "A";
    a.version = "1.0.0";
    a.dependencies = {{"B", ">=1.0.0", true}};
    plugins["A"] = a;

    auto order = resolver.resolveLoadOrder(plugins);

    // C 应该在 B 之前，B 应该在 A 之前
    auto c_pos = std::find(order.begin(), order.end(), "C");
    auto b_pos = std::find(order.begin(), order.end(), "B");
    auto a_pos = std::find(order.begin(), order.end(), "A");

    EXPECT_LT(c_pos, b_pos);
    EXPECT_LT(b_pos, a_pos);
}

TEST_F(DependencyResolverTest, CheckDependenciesSatisfied) {
    std::map<std::string, rpf::PluginMetadata> plugins;
    plugins["A"] = {"A", "1.0.0", "", "", "", {}, {}, "test", "all", "create_plugin"};
    plugins["B"] = {"B", "1.0.0", "", "", "", {{"A", ">=1.0.0", true}}, {}, "test", "all", "create_plugin"};

    EXPECT_TRUE(resolver.checkDependencies("B", plugins));
}

TEST_F(DependencyResolverTest, CheckDependenciesNotSatisfied) {
    std::map<std::string, rpf::PluginMetadata> plugins;
    plugins["B"] = {"B", "1.0.0", "", "", "", {{"A", ">=1.0.0", true}}, {}, "test", "all", "create_plugin"};

    EXPECT_FALSE(resolver.checkDependencies("B", plugins));
}

TEST_F(DependencyResolverTest, DetectCycles) {
    std::map<std::string, rpf::PluginMetadata> plugins;
    plugins["A"] = {"A", "1.0.0", "", "", "", {{"B", ">=1.0.0", true}}, {}, "test", "all", "create_plugin"};
    plugins["B"] = {"B", "1.0.0", "", "", "", {{"C", ">=1.0.0", true}}, {}, "test", "all", "create_plugin"};
    plugins["C"] = {"C", "1.0.0", "", "", "", {{"A", ">=1.0.0", true}}, {}, "test", "all", "create_plugin"};

    auto cycles = resolver.detectCycles(plugins);
    EXPECT_FALSE(cycles.empty());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
