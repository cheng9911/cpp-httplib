#include "rpf/plugin_manager.h"
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <mutex>

namespace fs = std::filesystem;

namespace rpf {

PluginManager::PluginManager(const std::string& plugin_dir)
    : plugin_dir_(plugin_dir),
      service_registry_(std::make_unique<ServiceRegistry>()),
      event_bus_(std::make_unique<EventBus>()),
      dependency_resolver_(std::make_unique<DependencyResolver>()) {
}

PluginManager::~PluginManager() {
    unloadAll();
}

std::vector<PluginMetadata> PluginManager::scanAvailablePlugins() {
    std::unique_lock lock(mutex_);
    metadata_cache_.clear();

    if (!fs::exists(plugin_dir_) || !fs::is_directory(plugin_dir_)) {
        return {};
    }

    std::vector<PluginMetadata> result;
    for (const auto& entry : fs::directory_iterator(plugin_dir_)) {
        if (!entry.is_regular_file()) continue;

        std::string filename = entry.path().filename().string();
        std::string ext = entry.path().extension().string();

        // 查找 .meta.json 文件
        if (ext == ".json" && filename.find(".meta.json") != std::string::npos) {
            try {
                auto meta = loadMetadataFromFile(entry.path().string());
                metadata_cache_[meta.name] = meta;
                result.push_back(meta);
            } catch (const std::exception& e) {
                std::cerr << "Failed to load metadata from " << entry.path()
                          << ": " << e.what() << std::endl;
            }
        }
    }

    return result;
}

PluginMetadata PluginManager::getPluginMetadata(const std::string& name) const {
    std::shared_lock lock(mutex_);
    auto it = metadata_cache_.find(name);
    if (it == metadata_cache_.end()) {
        throw std::runtime_error("Plugin not found: " + name);
    }
    return it->second;
}

std::string PluginManager::findMetaFile(const std::string& plugin_name) const {
    fs::path dir(plugin_dir_);
    fs::path meta_file = dir / (plugin_name + ".meta.json");
    if (fs::exists(meta_file)) {
        return meta_file.string();
    }
    return {};
}

std::string PluginManager::findLibFile(const std::string& plugin_name) const {
    fs::path dir(plugin_dir_);

    // 尝试不同的库文件名格式
    std::vector<std::string> candidates;
#ifdef _WIN32
    candidates = {
        plugin_name + ".dll",
        "lib" + plugin_name + ".dll",
        plugin_name + "_plugin.dll",
        "lib" + plugin_name + "_plugin.dll"
    };
#else
    candidates = {
        "lib" + plugin_name + ".so",
        plugin_name + ".so",
        "lib" + plugin_name + "_plugin.so",
        plugin_name + "_plugin.so"
    };
#endif

    for (const auto& candidate : candidates) {
        fs::path lib_path = dir / candidate;
        if (fs::exists(lib_path)) {
            return lib_path.string();
        }
    }

    // 如果找不到，尝试扫描目录中的所有.so文件
    if (fs::exists(dir)) {
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                std::string ext = entry.path().extension().string();
#ifdef _WIN32
                if (ext == ".dll" && filename.find(plugin_name) != std::string::npos) {
#else
                if (ext == ".so" && filename.find(plugin_name) != std::string::npos) {
#endif
                    return entry.path().string();
                }
            }
        }
    }

    return {};
}

bool PluginManager::loadPlugin(const std::string& name) {
    std::unique_lock lock(mutex_);

    // 检查是否已加载
    if (loaded_plugins_.count(name)) {
        return true;
    }

    // 加载元数据
    PluginMetadata meta;
    auto meta_it = metadata_cache_.find(name);
    if (meta_it != metadata_cache_.end()) {
        meta = meta_it->second;
    } else {
        std::string meta_file = findMetaFile(name);
        if (meta_file.empty()) {
            std::cerr << "Metadata file not found for plugin: " << name << std::endl;
            return false;
        }
        try {
            meta = loadMetadataFromFile(meta_file);
            metadata_cache_[name] = meta;
        } catch (const std::exception& e) {
            std::cerr << "Failed to load metadata: " << e.what() << std::endl;
            return false;
        }
    }

    // 检查依赖
    if (!dependency_resolver_->checkDependencies(name, metadata_cache_)) {
        std::cerr << "Dependencies not satisfied for plugin: " << name << std::endl;
        return false;
    }

    // 查找动态库
    std::string lib_file = findLibFile(name);
    if (lib_file.empty()) {
        std::cerr << "Library file not found for plugin: " << name << std::endl;
        return false;
    }

    // 加载动态库
    try {
        auto lib = std::make_unique<dylib>(lib_file, dylib::no_filename_decorations);

        auto create_func = lib->get_function<IPlugin*()>(meta.entry_point);
        auto destroy_func = lib->get_function<void(IPlugin*)>("destroy_plugin");

        LoadedPlugin loaded;
        loaded.metadata = meta;
        loaded.lib = std::move(lib);
        loaded.create_func = create_func;
        loaded.destroy_func = destroy_func;
        loaded.state = PluginState::Loaded;

        loaded_plugins_[name] = std::move(loaded);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load plugin library: " << e.what() << std::endl;
        return false;
    }
}

bool PluginManager::unloadPlugin(const std::string& name) {
    std::unique_lock lock(mutex_);

    auto it = loaded_plugins_.find(name);
    if (it == loaded_plugins_.end()) {
        return false;
    }

    auto& plugin = it->second;

    // 如果正在运行，先停止
    if (plugin.state == PluginState::Running) {
        if (plugin.instance) {
            plugin.instance->stop();
        }
    }

    // 如果已初始化，先卸载
    if (plugin.state != PluginState::Loaded && plugin.instance) {
        plugin.instance->unload();
    }

    // 销毁实例
    if (plugin.destroy_func && plugin.instance) {
        plugin.destroy_func(plugin.instance);
        plugin.instance = nullptr;
    }

    plugin.state = PluginState::Unloaded;
    loaded_plugins_.erase(it);
    return true;
}

bool PluginManager::initializePlugin(const std::string& name) {
    std::unique_lock lock(mutex_);

    auto it = loaded_plugins_.find(name);
    if (it == loaded_plugins_.end()) {
        return false;
    }

    auto& plugin = it->second;

    if (plugin.state != PluginState::Loaded) {
        return false;
    }

    // 创建插件实例
    if (!plugin.instance) {
        plugin.instance = plugin.create_func();
        if (!plugin.instance) {
            plugin.state = PluginState::Error;
            return false;
        }
    }

    if (plugin.instance->initialize()) {
        plugin.state = PluginState::Initialized;
        return true;
    }

    plugin.state = PluginState::Error;
    return false;
}

bool PluginManager::startPlugin(const std::string& name) {
    std::unique_lock lock(mutex_);

    auto it = loaded_plugins_.find(name);
    if (it == loaded_plugins_.end()) {
        return false;
    }

    auto& plugin = it->second;

    if (plugin.state != PluginState::Initialized) {
        return false;
    }

    if (plugin.instance->start()) {
        plugin.state = PluginState::Running;
        return true;
    }

    plugin.state = PluginState::Error;
    return false;
}

bool PluginManager::stopPlugin(const std::string& name) {
    std::unique_lock lock(mutex_);

    auto it = loaded_plugins_.find(name);
    if (it == loaded_plugins_.end()) {
        return false;
    }

    auto& plugin = it->second;

    if (plugin.state != PluginState::Running) {
        return false;
    }

    if (plugin.instance->stop()) {
        plugin.state = PluginState::Stopped;
        return true;
    }

    plugin.state = PluginState::Error;
    return false;
}

bool PluginManager::loadAll() {
    auto plugins = scanAvailablePlugins();

    // 解析加载顺序
    std::vector<std::string> load_order;
    try {
        load_order = dependency_resolver_->resolveLoadOrder(metadata_cache_);
    } catch (const std::exception& e) {
        std::cerr << "Failed to resolve load order: " << e.what() << std::endl;
        return false;
    }

    bool success = true;
    for (const auto& name : load_order) {
        if (!loadPlugin(name)) {
            std::cerr << "Failed to load plugin: " << name << std::endl;
            success = false;
        }
    }
    return success;
}

bool PluginManager::startAll() {
    std::shared_lock lock(mutex_);
    for (auto& [name, plugin] : loaded_plugins_) {
        if (plugin.state == PluginState::Loaded) {
            lock.unlock();
            if (!initializePlugin(name)) {
                return false;
            }
            lock.lock();
        }
        if (plugin.state == PluginState::Initialized) {
            lock.unlock();
            if (!startPlugin(name)) {
                return false;
            }
            lock.lock();
        }
    }
    return true;
}

bool PluginManager::stopAll() {
    std::unique_lock lock(mutex_);
    bool success = true;
    for (auto& [name, plugin] : loaded_plugins_) {
        if (plugin.state == PluginState::Running) {
            if (!plugin.instance->stop()) {
                success = false;
            } else {
                plugin.state = PluginState::Stopped;
            }
        }
    }
    return success;
}

bool PluginManager::unloadAll() {
    std::unique_lock lock(mutex_);
    for (auto& [name, plugin] : loaded_plugins_) {
        if (plugin.instance) {
            if (plugin.state == PluginState::Running) {
                plugin.instance->stop();
            }
            plugin.instance->unload();
            if (plugin.destroy_func) {
                plugin.destroy_func(plugin.instance);
            }
            plugin.instance = nullptr;
        }
        plugin.state = PluginState::Unloaded;
    }
    loaded_plugins_.clear();
    return true;
}

std::vector<std::string> PluginManager::getLoadedPlugins() const {
    std::shared_lock lock(mutex_);
    std::vector<std::string> result;
    for (const auto& [name, _] : loaded_plugins_) {
        result.push_back(name);
    }
    return result;
}

PluginState PluginManager::getPluginState(const std::string& name) const {
    std::shared_lock lock(mutex_);
    auto it = loaded_plugins_.find(name);
    if (it == loaded_plugins_.end()) {
        return PluginState::Unloaded;
    }
    return it->second.state;
}

IPlugin* PluginManager::getPlugin(const std::string& name) {
    std::shared_lock lock(mutex_);
    auto it = loaded_plugins_.find(name);
    if (it == loaded_plugins_.end()) {
        return nullptr;
    }
    return it->second.instance;
}

ServiceRegistry& PluginManager::getServiceRegistry() {
    return *service_registry_;
}

EventBus& PluginManager::getEventBus() {
    return *event_bus_;
}

// 扫描目录获取所有插件元数据 (自由函数实现)
std::vector<PluginMetadata> scanPluginDirectory(const std::string& dir) {
    std::vector<PluginMetadata> result;
    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        return result;
    }

    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;

        std::string filename = entry.path().filename().string();
        if (filename.find(".meta.json") != std::string::npos) {
            try {
                result.push_back(loadMetadataFromFile(entry.path().string()));
            } catch (const std::exception& e) {
                std::cerr << "Failed to load metadata: " << e.what() << std::endl;
            }
        }
    }
    return result;
}

} // namespace rpf
