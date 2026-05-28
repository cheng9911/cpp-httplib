#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <nlohmann/json.hpp>

namespace rpf {

struct Dependency {
    std::string name;
    std::string version_constraint;  // ">=1.0.0", "^2.0.0"
    bool required = true;
};

struct PluginMetadata {
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    std::string license;
    std::vector<Dependency> dependencies;
    std::vector<std::string> provides;
    std::string category;
    std::string platform = "all";
    std::string entry_point = "create_plugin";
};

// JSON 序列化/反序列化
inline void to_json(nlohmann::json& j, const Dependency& d) {
    j = {{"name", d.name}, {"version_constraint", d.version_constraint}, {"required", d.required}};
}

inline void from_json(const nlohmann::json& j, Dependency& d) {
    j.at("name").get_to(d.name);
    if (j.contains("version_constraint")) j.at("version_constraint").get_to(d.version_constraint);
    if (j.contains("required")) j.at("required").get_to(d.required);
}

inline void to_json(nlohmann::json& j, const PluginMetadata& m) {
    j = {
        {"name", m.name},
        {"version", m.version},
        {"description", m.description},
        {"author", m.author},
        {"license", m.license},
        {"dependencies", m.dependencies},
        {"provides", m.provides},
        {"category", m.category},
        {"platform", m.platform},
        {"entry_point", m.entry_point}
    };
}

inline void from_json(const nlohmann::json& j, PluginMetadata& m) {
    j.at("name").get_to(m.name);
    j.at("version").get_to(m.version);
    if (j.contains("description")) j.at("description").get_to(m.description);
    if (j.contains("author")) j.at("author").get_to(m.author);
    if (j.contains("license")) j.at("license").get_to(m.license);
    if (j.contains("dependencies")) j.at("dependencies").get_to(m.dependencies);
    if (j.contains("provides")) j.at("provides").get_to(m.provides);
    if (j.contains("category")) j.at("category").get_to(m.category);
    if (j.contains("platform")) j.at("platform").get_to(m.platform);
    if (j.contains("entry_point")) j.at("entry_point").get_to(m.entry_point);
}

// 从JSON文件加载元数据 (不需要加载插件)
inline PluginMetadata loadMetadataFromFile(const std::string& json_path) {
    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open metadata file: " + json_path);
    }
    nlohmann::json j;
    file >> j;
    return j.get<PluginMetadata>();
}

// 扫描目录获取所有插件元数据
std::vector<PluginMetadata> scanPluginDirectory(const std::string& dir);

} // namespace rpf
