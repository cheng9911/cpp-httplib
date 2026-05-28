#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include "plugin_metadata.h"

namespace rpf {

class DependencyResolver {
public:
    // 解析加载顺序 (拓扑排序)
    std::vector<std::string> resolveLoadOrder(
        const std::map<std::string, PluginMetadata>& plugins) const;

    // 检查依赖是否满足
    bool checkDependencies(
        const std::string& name,
        const std::map<std::string, PluginMetadata>& plugins) const;

    // 检测循环依赖
    std::vector<std::string> detectCycles(
        const std::map<std::string, PluginMetadata>& plugins) const;
};

} // namespace rpf
