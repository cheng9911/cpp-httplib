#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <shared_mutex>
#include <dylib.hpp>
#include "plugin_interface.h"
#include "plugin_metadata.h"
#include "service_registry.h"
#include "event_bus.h"
#include "dependency_resolver.h"

namespace rpf {

struct LoadedPlugin {
    PluginMetadata metadata;
    std::unique_ptr<dylib> lib;
    IPlugin* instance = nullptr;
    PluginCreateFunc create_func = nullptr;
    PluginDestroyFunc destroy_func = nullptr;
    PluginState state = PluginState::Unloaded;
};

class PluginManager {
public:
    explicit PluginManager(const std::string& plugin_dir);
    ~PluginManager();

    // 禁止拷贝
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    // 元数据扫描 (不加载插件)
    std::vector<PluginMetadata> scanAvailablePlugins();
    PluginMetadata getPluginMetadata(const std::string& name) const;

    // 插件生命周期
    bool loadPlugin(const std::string& name);
    bool unloadPlugin(const std::string& name);
    bool initializePlugin(const std::string& name);
    bool startPlugin(const std::string& name);
    bool stopPlugin(const std::string& name);

    // 批量操作
    bool loadAll();
    bool startAll();
    bool stopAll();
    bool unloadAll();

    // 查询
    std::vector<std::string> getLoadedPlugins() const;
    PluginState getPluginState(const std::string& name) const;
    IPlugin* getPlugin(const std::string& name);

    // 服务和事件
    ServiceRegistry& getServiceRegistry();
    EventBus& getEventBus();

private:
    std::string plugin_dir_;
    std::map<std::string, PluginMetadata> metadata_cache_;
    std::map<std::string, LoadedPlugin> loaded_plugins_;
    std::unique_ptr<ServiceRegistry> service_registry_;
    std::unique_ptr<EventBus> event_bus_;
    std::unique_ptr<DependencyResolver> dependency_resolver_;
    mutable std::shared_mutex mutex_;

    std::string findMetaFile(const std::string& plugin_name) const;
    std::string findLibFile(const std::string& plugin_name) const;
};

} // namespace rpf
