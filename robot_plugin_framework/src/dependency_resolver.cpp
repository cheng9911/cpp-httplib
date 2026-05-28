#include "rpf/dependency_resolver.h"
#include <algorithm>
#include <stdexcept>

namespace rpf {

std::vector<std::string> DependencyResolver::resolveLoadOrder(
    const std::map<std::string, PluginMetadata>& plugins) const {

    // Kahn's algorithm for topological sort
    std::map<std::string, int> in_degree;
    std::map<std::string, std::vector<std::string>> graph;

    // Initialize
    for (const auto& [name, _] : plugins) {
        in_degree[name] = 0;
        graph[name] = {};
    }

    // Build graph: dependency -> dependent
    for (const auto& [name, meta] : plugins) {
        for (const auto& dep : meta.dependencies) {
            if (plugins.count(dep.name) && dep.required) {
                graph[dep.name].push_back(name);
                in_degree[name]++;
            }
        }
    }

    // Find all nodes with no incoming edges
    std::vector<std::string> queue;
    for (const auto& [name, degree] : in_degree) {
        if (degree == 0) {
            queue.push_back(name);
        }
    }

    std::vector<std::string> result;
    while (!queue.empty()) {
        std::string current = queue.back();
        queue.pop_back();
        result.push_back(current);

        for (const auto& neighbor : graph[current]) {
            in_degree[neighbor]--;
            if (in_degree[neighbor] == 0) {
                queue.push_back(neighbor);
            }
        }
    }

    // Check for cycles
    if (result.size() != plugins.size()) {
        throw std::runtime_error("Circular dependency detected");
    }

    return result;
}

bool DependencyResolver::checkDependencies(
    const std::string& name,
    const std::map<std::string, PluginMetadata>& plugins) const {

    auto it = plugins.find(name);
    if (it == plugins.end()) {
        return false;
    }

    for (const auto& dep : it->second.dependencies) {
        if (dep.required && plugins.count(dep.name) == 0) {
            return false;
        }
    }

    return true;
}

std::vector<std::string> DependencyResolver::detectCycles(
    const std::map<std::string, PluginMetadata>& plugins) const {

    std::set<std::string> visited;
    std::set<std::string> rec_stack;
    std::vector<std::string> cycles;

    std::function<bool(const std::string&)> dfs = [&](const std::string& node) -> bool {
        visited.insert(node);
        rec_stack.insert(node);

        auto it = plugins.find(node);
        if (it != plugins.end()) {
            for (const auto& dep : it->second.dependencies) {
                if (dep.required && plugins.count(dep.name)) {
                    if (!visited.count(dep.name)) {
                        if (dfs(dep.name)) return true;
                    } else if (rec_stack.count(dep.name)) {
                        cycles.push_back(dep.name);
                        return true;
                    }
                }
            }
        }

        rec_stack.erase(node);
        return false;
    };

    for (const auto& [name, _] : plugins) {
        if (!visited.count(name)) {
            if (dfs(name)) break;
        }
    }

    return cycles;
}

} // namespace rpf
