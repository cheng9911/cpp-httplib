# 修改规划文档

## 一、问题分析总结

### 严重程度分类

#### 严重问题 (Critical)
1. **`startAll()` 死锁风险** - `plugin_manager.cpp` 中 `startAll()` 使用 `shared_lock` 后调用需要 `unique_lock` 的 `initializePlugin()` 和 `startPlugin()`，必然导致死锁
2. **路由正则表达式限制** - debug_api.cpp 中使用 `\w+` 模式，不匹配包含连字符的插件名（如 `my-plugin`），导致 API 路由失效
3. **ServiceRegistry 类型安全缺失** - `static_cast` 无类型检查，错误类型转换导致未定义行为

#### 中等问题 (Medium)
4. **EventBus 默认参数语义问题** - `nlohmann::json data = {}` 创建 null 而非空对象，可能导致下游代码误判
5. **EventBus handler 异常隔离不足** - 一个 handler 抛异常会中断后续 handler 执行
6. **Debug API 硬编码插件名** - `handleGetRobotState` 等函数硬编码 `"mujoco_hardware"`，不够灵活
7. **输入验证不足** - 关节名、服务名、事件名等缺乏输入验证
8. **API 安全性缺失** - CORS 允许所有来源、无认证机制
9. **线程安全问题** - `unloadPlugin()` 中先 `stop()` 再 `unload()` 的调用在持锁状态下进行，可能导致死锁

#### 轻微问题 (Low)
10. **CORS 配置过于宽松** - `Access-Control-Allow-Origin: *` 在生产环境不安全
11. **日志输出不统一** - 部分使用 `std::cout`，部分使用 `std::cerr`，缺乏统一日志框架
12. **错误处理不一致** - 部分 API 返回 HTTP 400，部分返回 HTTP 200 + `{"success": false}`
13. **测试覆盖不足** - 91 个测试用例被跳过

---

## 二、修改计划

### 1. 修复 `startAll()` 死锁问题 - 优先级 P0

**问题描述**：
`PluginManager::startAll()` 在 `plugin_manager.cpp` 第 315-334 行使用 `std::shared_lock lock(mutex_)` 获取共享锁，然后调用 `initializePlugin(name)` 和 `startPlugin(name)`，这两个函数内部会尝试获取 `std::unique_lock lock(mutex_)` 排他锁。由于共享锁已被持有，排他锁无法获取，导致死锁。

**影响范围**：
- `startAll()` 函数完全不可用
- 任何依赖 `startAll()` 的代码都会死锁
- Debug server 的 `main.cpp` 中自动启动插件的流程不受影响（使用逐个调用方式）

**修改方案**：
修改 `startAll()` 实现，避免在持锁状态下调用需要锁的函数。有两种方案：

方案 A（推荐）：收集需要操作的插件列表，释放锁后再逐个操作：
```cpp
bool PluginManager::startAll() {
    // 先收集需要初始化和启动的插件名
    std::vector<std::string> to_initialize;
    std::vector<std::string> to_start;
    {
        std::shared_lock lock(mutex_);
        for (auto& [name, plugin] : loaded_plugins_) {
            if (plugin.state == PluginState::Loaded) {
                to_initialize.push_back(name);
            }
        }
    }

    // 逐个初始化
    for (const auto& name : to_initialize) {
        if (!initializePlugin(name)) {
            return false;
        }
    }

    // 逐个启动
    {
        std::shared_lock lock(mutex_);
        for (auto& [name, plugin] : loaded_plugins_) {
            if (plugin.state == PluginState::Initialized) {
                to_start.push_back(name);
            }
        }
    }
    for (const auto& name : to_start) {
        if (!startPlugin(name)) {
            return false;
        }
    }
    return true;
}
```

**涉及文件**：
- `/home/sun/Documents/GitHub/cpp-httplib/robot_plugin_framework/src/plugin_manager.cpp` (第 315-334 行)

**预计工作量**：0.5 小时

**风险评估**：低。修改只影响 `startAll()` 的内部实现，不改变外部接口和语义。

---

### 2. 修复路由正则表达式限制 - 优先级 P0

**问题描述**：
`debug_api.cpp` 中所有路由使用 `\w+` 正则模式（第 79-101 行），`\w` 只匹配 `[a-zA-Z0-9_]`，不匹配连字符 `-`。如果插件名包含连字符（如 `my-plugin`、`motion-control`），API 路由将无法匹配。

**影响范围**：
- 所有使用 `(\w+)` 捕获组的 API 端点
- 插件名包含连字符时，`/api/plugins/{name}/load` 等端点返回 404

**修改方案**：
将 `\w+` 替换为 `[^/]+` 或 `[a-zA-Z0-9_-]+`：

```cpp
// 修改前
server_->Get(R"(/api/plugins/(\w+)/metadata)", ...);
// 修改后
server_->Get(R"(/api/plugins/([^/]+)/metadata)", ...);
```

需要修改的路由：
- `/api/plugins/(\w+)/metadata` -> `/api/plugins/([^/]+)/metadata`
- `/api/plugins/(\w+)/load` -> `/api/plugins/([^/]+)/load`
- `/api/plugins/(\w+)/unload` -> `/api/plugins/([^/]+)/unload`
- `/api/plugins/(\w+)/start` -> `/api/plugins/([^/]+)/start`
- `/api/plugins/(\w+)/stop` -> `/api/plugins/([^/]+)/stop`
- `/api/plugins/(\w+)/state` -> `/api/plugins/([^/]+)/state`
- `/api/robot/joints/(\w+)/position` -> `/api/robot/joints/([^/]+)/position`

**涉及文件**：
- `/home/sun/Documents/GitHub/cpp-httplib/robot_plugin_framework/examples/debug_server/debug_api.cpp` (第 79-114 行)

**预计工作量**：0.5 小时

**风险评估**：低。`[^/]+` 比 `\w+` 更宽松，不会破坏现有功能，只是扩展了支持的字符范围。

---

### 3. ServiceRegistry 类型安全增强 - 优先级 P0

**问题描述**：
`service_registry.h` 中 `getService<T>()` 使用 `std::static_pointer_cast<T>(it->second)` 进行类型转换，没有任何类型检查。如果注册时使用类型 A，获取时使用类型 B，会导致未定义行为（通常是内存损坏或段错误）。

**影响范围**：
- 所有使用 `getService<T>()` 的代码
- 插件间服务通信的正确性

**修改方案**：
添加类型信息存储和检查机制：

```cpp
class ServiceRegistry {
public:
    template<typename T>
    bool registerService(const std::string& name, std::shared_ptr<T> service) {
        std::unique_lock lock(mutex_);
        services_[name] = std::static_pointer_cast<void>(service);
        type_ids_[name] = std::type_index(typeid(T));
        return true;
    }

    template<typename T>
    std::shared_ptr<T> getService(const std::string& name) const {
        std::shared_lock lock(mutex_);
        auto it = services_.find(name);
        if (it == services_.end()) {
            return nullptr;
        }
        // 类型检查
        auto type_it = type_ids_.find(name);
        if (type_it != type_ids_.end() && type_it->second != std::type_index(typeid(T))) {
            return nullptr;  // 类型不匹配
        }
        return std::static_pointer_cast<T>(it->second);
    }

private:
    std::map<std::string, std::shared_ptr<void>> services_;
    std::map<std::string, std::type_index> type_ids_;
    mutable std::shared_mutex mutex_;
};
```

**涉及文件**：
- `/home/sun/Documents/GitHub/cpp-httplib/robot_plugin_framework/include/rpf/service_registry.h`

**预计工作量**：1 小时

**风险评估**：中等。添加类型检查可能改变现有行为（之前错误类型转换可能"恰好"工作），需要验证所有现有代码。

---

### 4. EventBus 异常隔离和默认参数修复 - 优先级 P1

**问题描述**：
1. `publish()` 函数中，如果某个 handler 抛出异常，后续 handler 不会被调用
2. 默认参数 `nlohmann::json data = {}` 创建 null 类型而非空对象 `{}`

**影响范围**：
- 事件系统的可靠性
- 依赖默认 data 参数的下游代码

**修改方案**：

```cpp
// 修复异常隔离
void publish(const std::string& event, const nlohmann::json& data = nlohmann::json::object()) {
    std::shared_lock lock(mutex_);
    auto it = handlers_.find(event);
    if (it != handlers_.end()) {
        for (const auto& sub : it->second) {
            try {
                sub.handler(event, data);
            } catch (const std::exception& e) {
                // 记录错误但继续执行其他 handler
                std::cerr << "EventBus: handler exception for event '" 
                          << event << "': " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "EventBus: unknown handler exception for event '" 
                          << event << "'" << std::endl;
            }
        }
    }
}
```

**涉及文件**：
- `/home/sun/Documents/GitHub/cpp-httplib/robot_plugin_framework/include/rpf/event_bus.h` (第 45-53 行)

**预计工作量**：0.5 小时

**风险评估**：低。异常隔离是纯增量改进，不影响正常流程。默认参数修改可能影响依赖 null 行为的代码，但测试已确认当前行为。

---

### 5. Debug API 硬编码插件名解耦 - 优先级 P1

**问题描述**：
`debug_api.cpp` 中 `handleGetRobotState()`、`handleSetJointPosition()` 等函数硬编码 `"mujoco_hardware"` 作为硬件插件名。如果使用其他硬件插件，这些 API 将无法工作。

**影响范围**：
- 所有硬件相关 API 端点
- 系统可扩展性

**修改方案**：
添加硬件插件名配置或自动发现机制：

```cpp
class DebugAPI {
public:
    DebugAPI(rpf::PluginManager& plugin_manager, int port = 8080,
             const std::string& hardware_plugin = "mujoco_hardware");
    // ...
private:
    std::string hardware_plugin_name_;
    // ...
};

// 使用 hardware_plugin_name_ 替代硬编码的 "mujoco_hardware"
void DebugAPI::handleGetRobotState(const httplib::Request& req, httplib::Response& res) {
    auto* hardware_plugin = plugin_manager_.getPlugin(hardware_plugin_name_);
    // ...
}
```

**涉及文件**：
- `/home/sun/Documents/GitHub/cpp-httplib/robot_plugin_framework/examples/debug_server/debug_api.h`
- `/home/sun/Documents/GitHub/cpp-httplib/robot_plugin_framework/examples/debug_server/debug_api.cpp`
- `/home/sun/Documents/GitHub/cpp-httplib/robot_plugin_framework/examples/debug_server/main.cpp`

**预计工作量**：1 小时

**风险评估**：低。添加配置参数，向后兼容默认值 `"mujoco_hardware"`。

---

### 6. 输入验证增强 - 优先级 P1

**问题描述**：
API 端点缺乏输入验证：
- 关节名未验证是否为有效关节
- 服务名未验证（空字符串可注册）
- 事件名未验证
- 仿真步长 dt 未验证（可为负数或零）

**影响范围**：
- API 健壮性
- 潜在的安全风险

**修改方案**：
在 API handler 中添加输入验证：

```cpp
// 关节名验证
void DebugAPI::handleSetJointPosition(const httplib::Request& req, httplib::Response& res) {
    std::string joint_name = req.matches[1];
    
    // 验证关节名格式
    if (joint_name.empty() || joint_name.length() > 64) {
        res.status = 400;
        res.set_content(R"({"error": "Invalid joint name"})", "application/json");
        return;
    }
    
    // 验证 position 值范围
    try {
        auto body = nlohmann::json::parse(req.body);
        double position = body["position"];
        if (std::isnan(position) || std::isinf(position)) {
            res.status = 400;
            res.set_content(R"({"error": "Invalid position value"})", "application/json");
            return;
        }
        // ...
    }
}

// 仿真步长验证
void DebugAPI::handleStepSimulation(const httplib::Request& req, httplib::Response& res) {
    // ...
    if (!req.body.empty()) {
        try {
            auto body = nlohmann::json::parse(req.body);
            if (body.contains("dt")) {
                dt = body["dt"].get<double>();
                if (dt <= 0.0 || dt > 10.0) {  // 合理范围检查
                    res.status = 400;
                    res.set_content(R"({"error": "dt must be in (0, 10]"})", "application/json");
                    return;
                }
            }
        } catch (...) {}
    }
    // ...
}
```

**涉及文件**：
- `/home/sun/Documents/GitHub/cpp-httplib/robot_plugin_framework/examples/debug_server/debug_api.cpp`
- `/home/sun/Documents/GitHub/cpp-httplib/robot_plugin_framework/include/rpf/service_registry.h` (registerService 添加空名检查)

**预计工作量**：2 小时

**风险评估**：中等。添加验证可能改变现有 API 行为，需要更新测试用例。

---

### 7. PluginManager 线程安全修复 - 优先级 P1

**问题描述**：
`unloadPlugin()` 函数在持有 `unique_lock` 的情况下调用 `plugin.instance->stop()` 和 `plugin.instance->unload()`。如果这些函数内部尝试获取锁（虽然当前实现没有），会导致死锁。更严重的是，`stopAll()` 直接调用 `plugin.instance->stop()` 而不通过 `stopPlugin()`，绕过了状态检查。

**影响范围**：
- 插件卸载流程的安全性
- 并发场景下的正确性

**修改方案**：
重构 `unloadPlugin()` 和 `unloadAll()`，先收集信息再释放锁后操作：

```cpp
bool PluginManager::unloadPlugin(const std::string& name) {
    LoadedPlugin plugin_copy;
    bool found = false;
    
    {
        std::unique_lock lock(mutex_);
        auto it = loaded_plugins_.find(name);
        if (it == loaded_plugins_.end()) {
            return false;
        }
        // 移动出插件信息
        plugin_copy = std::move(it->second);
        loaded_plugins_.erase(it);
        found = true;
    }
    
    // 在锁外执行插件操作
    if (found && plugin_copy.instance) {
        if (plugin_copy.state == PluginState::Running) {
            plugin_copy.instance->stop();
        }
        if (plugin_copy.state != PluginState::Loaded) {
            plugin_copy.instance->unload();
        }
        if (plugin_copy.destroy_func) {
            plugin_copy.destroy_func(plugin_copy.instance);
        }
    }
    
    return found;
}
```

**涉及文件**：
- `/home/sun/Documents/GitHub/cpp-httplib/robot_plugin_framework/src/plugin_manager.cpp`

**预计工作量**：2 小时

**风险评估**：中等。重构锁的使用模式，需要仔细测试并发场景。

---

### 8. API 错误响应格式统一 - 优先级 P2

**问题描述**：
API 错误响应格式不一致：
- 部分返回 HTTP 400 + `{"error": "..."}`
- 部分返回 HTTP 200 + `{"success": false}`
- 部分返回 HTTP 404 + `{"error": "..."}`

**影响范围**：
- API 一致性和易用性
- 客户端错误处理逻辑

**修改方案**：
定义统一的错误响应格式：

```cpp
// 统一错误响应
void sendError(httplib::Response& res, int status, const std::string& message) {
    res.status = status;
    nlohmann::json error = {{"error", message}, {"success", false}};
    res.set_content(error.dump(2), "application/json");
}

void sendSuccess(httplib::Response& res, const nlohmann::json& data = {}) {
    nlohmann::json result = {{"success", true}};
    result.update(data);
    res.set_content(result.dump(2), "application/json");
}
```

统一规则：
- 400: 请求格式错误（无效 JSON、缺少字段）
- 404: 资源不存在（插件未加载）
- 500: 服务器内部错误
- 200: 成功（包括幂等操作如重复停止）

**涉及文件**：
- `/home/sun/Documents/GitHub/cpp-httplib/robot_plugin_framework/examples/debug_server/debug_api.cpp`
- `/home/sun/Documents/GitHub/cpp-httplib/robot_plugin_framework/examples/debug_server/debug_api.h`

**预计工作量**：2 小时

**风险评估**：中等。改变 API 响应格式可能影响现有客户端代码（包括 Web UI JavaScript）。

---

### 9. CORS 安全配置 - 优先级 P2

**问题描述**：
当前 CORS 配置允许所有来源（`Access-Control-Allow-Origin: *`），在生产环境存在安全风险。

**影响范围**：
- API 安全性
- 跨域请求控制

**修改方案**：
添加可配置的 CORS 策略：

```cpp
class DebugAPI {
public:
    struct CorsConfig {
        std::string allowed_origins = "*";  // 默认允许所有（开发模式）
        std::string allowed_methods = "GET, POST, PUT, DELETE, OPTIONS";
        std::string allowed_headers = "Content-Type, Authorization";
        int max_age = 86400;
    };
    
    DebugAPI(rpf::PluginManager& plugin_manager, int port = 8080,
             const CorsConfig& cors = CorsConfig{});
    // ...
private:
    CorsConfig cors_config_;
};
```

在 `setupRoutes()` 中使用配置：
```cpp
server_->Options(R"(/api/.*)", [this](const httplib::Request&, httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin", cors_config_.allowed_origins);
    res.set_header("Access-Control-Allow-Methods", cors_config_.allowed_methods);
    res.set_header("Access-Control-Allow-Headers", cors_config_.allowed_headers);
    res.set_header("Access-Control-Max-Age", std::to_string(cors_config_.max_age));
    res.status = 204;
});
```

**涉及文件**：
- `/home/sun/Documents/GitHub/cpp-httplib/robot_plugin_framework/examples/debug_server/debug_api.h`
- `/home/sun/Documents/GitHub/cpp-httplib/robot_plugin_framework/examples/debug_server/debug_api.cpp`

**预计工作量**：1 小时

**风险评估**：低。默认配置保持现有行为，只是添加了可配置性。

---

### 10. 统一日志框架 - 优先级 P2

**问题描述**：
代码中日志输出不统一：
- 部分使用 `std::cout`
- 部分使用 `std::cerr`
- 缺乏日志级别控制
- 缺乏时间戳和上下文信息

**影响范围**：
- 调试和问题排查效率
- 生产环境日志管理

**修改方案**：
创建简单的日志工具类：

```cpp
// include/rpf/logger.h
namespace rpf {

enum class LogLevel { DEBUG, INFO, WARN, ERROR };

class Logger {
public:
    static Logger& instance() {
        static Logger logger;
        return logger;
    }
    
    void setLevel(LogLevel level) { level_ = level; }
    
    template<typename... Args>
    void log(LogLevel level, const std::string& module, Args&&... args) {
        if (level < level_) return;
        std::lock_guard lock(mutex_);
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        
        std::ostream& out = (level >= LogLevel::WARN) ? std::cerr : std::cout;
        out << "[" << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") << "] "
            << "[" << levelToString(level) << "] "
            << "[" << module << "] ";
        (out << ... << std::forward<Args>(args));
        out << std::endl;
    }

private:
    LogLevel level_ = LogLevel::INFO;
    std::mutex mutex_;
    
    const char* levelToString(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO:  return "INFO";
            case LogLevel::WARN:  return "WARN";
            case LogLevel::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }
};

#define RPF_LOG_DEBUG(module, ...) rpf::Logger::instance().log(rpf::LogLevel::DEBUG, module, __VA_ARGS__)
#define RPF_LOG_INFO(module, ...)  rpf::Logger::instance().log(rpf::LogLevel::INFO, module, __VA_ARGS__)
#define RPF_LOG_WARN(module, ...)  rpf::Logger::instance().log(rpf::LogLevel::WARN, module, __VA_ARGS__)
#define RPF_LOG_ERROR(module, ...) rpf::Logger::instance().log(rpf::LogLevel::ERROR, module, __VA_ARGS__)

} // namespace rpf
```

**涉及文件**：
- 新建 `/home/sun/Documents/GitHub/cpp-httplib/robot_plugin_framework/include/rpf/logger.h`
- 修改所有使用 `std::cout`/`std::cerr` 的文件

**预计工作量**：3 小时

**风险评估**：低。纯增量改进，不影响功能。

---

## 三、测试补充计划

### 1. Web UI 测试 (61 个用例)

**方案**：使用 Selenium WebDriver 或 Playwright 进行自动化测试

**实施步骤**：
1. 安装测试工具（Selenium + ChromeDriver 或 Playwright）
2. 编写测试脚本，覆盖 W-001 ~ W-061
3. 集成到 CI/CD 流程

**优先级**：P1（核心功能已通过 API 测试验证，Web UI 是展示层）

**预计工作量**：8-16 小时

### 2. P2 优先级单元测试 (8 个用例)

| 用例 | 补充方案 | 工作量 |
|------|----------|--------|
| D-017 多个独立环 | 编写测试用例，构造两个独立循环依赖 | 0.5h |
| E-012 handler 异常隔离 | 编写测试，验证异常不影响其他 handler | 0.5h |
| E-014 大量订阅者 | 编写性能测试，1000 个 handler | 0.5h |
| S-011 类型转换错误 | 编写测试，验证类型不匹配返回 nullptr | 0.5h |
| PM-040 路径含空格 | 编写测试，使用含空格的临时目录 | 0.5h |
| API-040 dt=0 | 编写 curl 测试脚本 | 0.25h |
| API-041 负 dt | 编写 curl 测试脚本 | 0.25h |
| API-057 大请求体 | 编写测试，发送 >1MB JSON | 0.5h |

### 3. 特殊场景测试 (3 个用例)

| 用例 | 补充方案 | 工作量 |
|------|----------|--------|
| PM-014 依赖不满足 | 创建带依赖的 meta.json，验证加载失败 | 1h |
| PM-027 循环依赖 | 创建循环依赖的 meta.json 文件 | 1h |
| API-002 空目录扫描 | 重启服务器时使用空目录，或添加目录配置 API | 1h |

### 4. 并发和压力测试 (6 个用例)

| 用例 | 补充方案 | 工作量 |
|------|----------|--------|
| PM-038 并发操作 | 使用 std::thread 并发调用 load/unload/getState | 2h |
| API-055 无效方法 | 使用 curl 发送 DELETE 请求 | 0.25h |
| API-056 并发请求 | 编写并发 curl 脚本或使用 ab/wrk | 1h |
| API-049 缺少 event 字段 | 编写 curl 测试 | 0.25h |
| API-060/061/062 服务器生命周期 | 编写测试程序，多次 start/stop | 2h |
| API-063 端口占用 | 编写测试，启动两个实例 | 1h |

### 5. 测试基础设施改进

1. **添加测试配置**：通过环境变量或配置文件指定插件目录路径
2. **添加测试 fixtures**：创建可复用的测试夹具，减少重复代码
3. **添加集成测试**：测试完整的 API 工作流（扫描 -> 加载 -> 启动 -> 操作 -> 停止 -> 卸载）
4. **添加性能基准测试**：使用 Google Benchmark 库

---

## 四、长期改进建议

### 1. 架构优化

#### 1.1 插件发现机制改进
- **当前**：扫描目录中的 `.meta.json` 文件
- **改进**：支持插件注册表（JSON 文件或数据库），支持远程插件仓库
- **收益**：更好的插件管理和版本控制

#### 1.2 服务注册表增强
- **当前**：简单的名称-指针映射
- **改进**：支持服务版本、服务契约、服务依赖
- **收益**：更好的服务管理和兼容性

#### 1.3 事件系统增强
- **当前**：简单的发布-订阅
- **改进**：支持事件过滤、事件优先级、异步事件处理
- **收益**：更灵活的事件驱动架构

### 2. 性能优化

#### 2.1 锁优化
- **当前**：全局共享锁保护所有操作
- **改进**：细粒度锁（per-plugin 锁），读写锁分离
- **收益**：提高并发性能

#### 2.2 内存管理
- **当前**：插件实例使用原始指针
- **改进**：使用智能指针（unique_ptr/shared_ptr）
- **收益**：更好的内存安全

#### 2.3 异步 API
- **当前**：同步 API 调用
- **改进**：支持异步操作和回调
- **收益**：更好的响应性和吞吐量

### 3. 功能扩展

#### 3.1 插件热重载
- 支持在运行时重新加载插件（卸载旧版本、加载新版本）
- 需要处理状态迁移和依赖更新

#### 3.2 插件沙箱
- 隔离插件执行环境，防止插件崩溃影响主进程
- 可以使用进程隔离或 WebAssembly

#### 3.3 配置管理
- 支持插件配置文件（JSON/YAML）
- 支持运行时配置更新
- 支持配置版本控制

#### 3.4 监控和指标
- 添加性能指标收集（CPU、内存、响应时间）
- 支持 Prometheus/Grafana 集成
- 添加健康检查端点

### 4. 开发体验改进

#### 4.1 文档完善
- 添加 API 文档（OpenAPI/Swagger）
- 添加插件开发指南
- 添加架构设计文档

#### 4.2 工具链
- 添加插件脚手架工具（生成插件模板）
- 添加插件验证工具（检查 meta.json 和接口实现）
- 添加调试工具（插件状态查看、事件追踪）

#### 4.3 CI/CD 集成
- 添加自动化构建和测试
- 添加代码覆盖率报告
- 添加静态分析（clang-tidy, cppcheck）

---

## 五、修改优先级排序

按紧急程度和影响范围排序：

### 第一阶段：关键修复 (1-2 天)

| 优先级 | 问题 | 工作量 | 文件 |
|--------|------|--------|------|
| P0 | 1. 修复 startAll() 死锁 | 0.5h | plugin_manager.cpp |
| P0 | 2. 修复路由正则表达式 | 0.5h | debug_api.cpp |
| P0 | 3. ServiceRegistry 类型安全 | 1h | service_registry.h |

### 第二阶段：重要改进 (3-5 天)

| 优先级 | 问题 | 工作量 | 文件 |
|--------|------|--------|------|
| P1 | 4. EventBus 异常隔离 | 0.5h | event_bus.h |
| P1 | 5. Debug API 硬编码解耦 | 1h | debug_api.h/cpp, main.cpp |
| P1 | 6. 输入验证增强 | 2h | debug_api.cpp |
| P1 | 7. PluginManager 线程安全 | 2h | plugin_manager.cpp |

### 第三阶段：质量提升 (1-2 周)

| 优先级 | 问题 | 工作量 | 文件 |
|--------|------|--------|------|
| P2 | 8. API 错误响应统一 | 2h | debug_api.cpp |
| P2 | 9. CORS 安全配置 | 1h | debug_api.h/cpp |
| P2 | 10. 统一日志框架 | 3h | 新建 logger.h + 多文件 |
| P2 | 测试补充 (P2 用例) | 4h | tests/ |
| P2 | 测试补充 (特殊场景) | 3h | tests/ |

### 第四阶段：长期改进 (持续)

| 类别 | 改进项 | 预计工作量 |
|------|--------|------------|
| 测试 | Web UI 自动化测试 | 16h |
| 测试 | 并发和压力测试 | 8h |
| 架构 | 插件发现机制改进 | 16h |
| 架构 | 服务注册表增强 | 8h |
| 性能 | 锁优化 | 8h |
| 功能 | 插件热重载 | 24h |
| 功能 | 配置管理 | 16h |
| 工具 | 文档和工具链 | 24h |

---

## 附录：修改影响分析

### 向后兼容性

| 修改项 | 兼容性 | 说明 |
|--------|--------|------|
| startAll() 死锁修复 | 完全兼容 | 修复 bug，不改变接口 |
| 路由正则表达式 | 完全兼容 | 扩展支持范围，不影响现有插件名 |
| ServiceRegistry 类型安全 | 可能不兼容 | 错误类型转换之前"可能"工作，现在返回 nullptr |
| EventBus 默认参数 | 可能不兼容 | 依赖 null 行为的代码需要更新 |
| Debug API 硬编码解耦 | 完全兼容 | 添加默认值，保持现有行为 |
| 输入验证增强 | 可能不兼容 | 之前接受的无效输入现在会被拒绝 |
| API 错误响应统一 | 可能不兼容 | 响应格式变化，客户端需要更新 |
| CORS 安全配置 | 完全兼容 | 默认配置保持现有行为 |

### 测试策略

1. **修改前**：运行完整测试套件，记录基线
2. **修改中**：每个修改项完成后运行相关测试
3. **修改后**：运行完整测试套件，验证无回归
4. **补充测试**：根据修改内容补充测试用例

### 风险缓解

1. **代码审查**：所有修改需要代码审查
2. **渐进式发布**：按优先级分阶段发布
3. **回滚计划**：保留修改前的代码版本
4. **监控告警**：添加关键指标监控
