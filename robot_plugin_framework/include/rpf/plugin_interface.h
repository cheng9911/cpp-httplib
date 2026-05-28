#pragma once

#include <string>
#include <memory>
#include <functional>

#ifdef _WIN32
    #ifdef RPF_EXPORTS
        #define PLUGIN_API __declspec(dllexport)
    #else
        #define PLUGIN_API __declspec(dllimport)
    #endif
#else
    #define PLUGIN_API __attribute__((visibility("default")))
#endif

namespace rpf {

enum class PluginState {
    Unloaded,
    Loaded,
    Initialized,
    Running,
    Stopped,
    Error
};

inline const char* pluginStateToString(PluginState state) {
    switch (state) {
        case PluginState::Unloaded: return "Unloaded";
        case PluginState::Loaded: return "Loaded";
        case PluginState::Initialized: return "Initialized";
        case PluginState::Running: return "Running";
        case PluginState::Stopped: return "Stopped";
        case PluginState::Error: return "Error";
        default: return "Unknown";
    }
}

class IPlugin {
public:
    virtual ~IPlugin() = default;

    // 生命周期
    virtual bool initialize() = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual void unload() = 0;

    // 状态查询
    virtual PluginState getState() const = 0;
    virtual std::string getName() const = 0;
    virtual std::string getVersion() const = 0;

    // 服务接口 (可选覆盖)
    virtual void* getService(const std::string& /*name*/) { return nullptr; }
};

using PluginCreateFunc = IPlugin* (*)();
using PluginDestroyFunc = void (*)(IPlugin*);

} // namespace rpf

// 宏定义，用于简化插件导出
#define RPF_PLUGIN_EXPORT(PluginClass) \
    extern "C" PLUGIN_API rpf::IPlugin* create_plugin() { \
        return new PluginClass(); \
    } \
    extern "C" PLUGIN_API void destroy_plugin(rpf::IPlugin* plugin) { \
        delete plugin; \
    }
