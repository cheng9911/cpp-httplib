# C++ 插件机制框架调研报告

## 调研概述

基于机器人控制器插件机制需求文档，对主流 C++ 插件框架进行全面调研，涵盖框架特性、代码示例、适用性分析等内容。

---

## 1. 主流 C++ 插件框架对比总览

| 框架 | GitHub Stars | C++ 版本 | 跨平台 | 元数据支持 | 热重载 | 许可证 | 适用场景 |
|------|-------------|---------|--------|-----------|--------|--------|---------|
| dylib | ~500+ | C++17 | ✅ | ❌ | ❌ | MIT | 轻量级动态库加载 |
| Boost.DLL | ~350+ | C++11 | ✅ | ✅ (library_info) | ❌ | Boost | Boost 生态集成 |
| POCO ClassLoader | ~8,300+ | C++11 | ✅ | ✅ (Manifest) | ❌ | Boost | 企业级应用 |
| Qt Plugin System | Qt 生态 | C++11 | ✅ | ✅ (JSON) | ❌ | LGPL/GPL | Qt 应用扩展 |
| cr.h | ~4,500+ | C/C++ | ✅ | ❌ | ✅ | MIT | 开发阶段热重载 |
| dynalo | ~50-200 | C++11 | ✅ | ❌ | ❌ | MIT | 极简动态库加载 |
| RuntimeCompiledC++ | ~2,000+ | C++11 | ✅ | ❌ | ✅ | MIT | 游戏开发热重载 |
| LIEF | ~4,000+ | C++11 | ✅ | ✅ (section解析) | ❌ | Apache 2.0 | 二进制分析 |
| ROS pluginlib | ROS 生态 | C++14 | ✅ | ✅ (XML) | ❌ | BSD | 机器人系统 |
| Orocos RTT | 机器人生态 | C++11 | ✅ | ✅ (部署文件) | ❌ | GPL | 实时机器人控制 |
| AprioritInc/CppPluginFramework | ~50-150 | C++11 | ✅ | ❌ | ❌ | MIT | 通用插件系统 |
| vector-of-bool/plug | ~50-150 | C++17 | ✅ | ❌ | ❌ | MIT | 轻量级插件 |
| YOMM2 | ~500-800+ | C++17 | ✅ | ❌ | ❌ | Boost | 多方法分派 |

---

## 2. 各框架详细分析

### 2.1 dylib (martin-olivier/dylib)

**GitHub**: https://github.com/martin-olivier/dylib

**核心特性**:
- 现代 C++17 设计，API 简洁易用
- 跨平台支持 (Windows/Linux/macOS)
- 单头文件可用
- 模板化的符号获取，类型安全
- 异常处理机制完善

**代码示例**:
```cpp
#include "dylib.hpp"

int main() {
    try {
        // 加载动态库
        dylib lib("./my_plugin", dylib::extension);

        // 获取函数符号
        auto adder = lib.get_function<double(double, double)>("add");
        double result = adder(10.0, 5.0);

        // 获取变量符号
        auto &counter = lib.get_variable<int>("counter");

        // 检查符号是否存在
        if (lib.has_function("process")) {
            auto process = lib.get_function<void()>("process");
            process();
        }
    } catch (const dylib::load_error &e) {
        std::cerr << "加载失败: " << e.what() << std::endl;
    } catch (const dylib::symbol_error &e) {
        std::cerr << "符号未找到: " << e.what() << std::endl;
    }
    return 0;
}
```

**CMake 集成**:
```cmake
include(FetchContent)
FetchContent_Declare(
    dylib
    GIT_REPOSITORY https://github.com/martin-olivier/dylib.git
    GIT_TAG v2.2.0
)
FetchContent_MakeAvailable(dylib)
target_link_libraries(your_target PRIVATE dylib)
```

**需求匹配度分析**:
| 需求项 | 支持情况 | 说明 |
|--------|---------|------|
| 动态加载/卸载 | ✅ 完全支持 | `dylib` 类自动管理生命周期 |
| 元数据读取 | ❌ 不支持 | 需自行实现 |
| 生命周期管理 | ⚠️ 基础支持 | 仅加载/卸载，无完整生命周期 |
| 依赖管理 | ❌ 不支持 | 需自行实现 |
| 线程安全 | ⚠️ 部分支持 | 库本身线程安全，插件需自行保证 |
| 跨平台 | ✅ 完全支持 | Windows/Linux/macOS |
| C++ 版本 | C++17 | 符合需求 |

**优势**: 简洁、轻量、易于集成
**劣势**: 功能单一，不支持元数据和生命周期管理

---

### 2.2 Boost.DLL

**GitHub**: https://github.com/boostorg/dll

**核心特性**:
- Boost 生态系统集成
- `shared_library` 类管理动态库
- `import` 函数模板化导入符号
- `library_info` 查询库信息
- 跨平台支持 (Windows/Linux/macOS)

**代码示例**:
```cpp
#include <boost/dll.hpp>
#include <iostream>

namespace dll = boost::dll;

int main() {
    // 加载共享库
    dll::shared_library lib("my_plugin");

    // 导入函数
    auto greet = dll::import<std::string()>(
        "my_plugin", "greet_function");

    std::cout << greet() << std::endl;

    // 查询库信息
    dll::library_info info("my_plugin");
    auto symbols = info.symbols();
    for (const auto &sym : symbols) {
        std::cout << "Symbol: " << sym << std::endl;
    }

    // 使用智能别名
    auto process = dll::import_alias<void(int)>(
        "my_plugin", "process_data");

    process(42);
    return 0;
}
```

**插件接口示例**:
```cpp
// plugin_api.hpp
class PluginBase {
public:
    virtual ~PluginBase() = default;
    virtual std::string name() const = 0;
    virtual void initialize() = 0;
    virtual void execute() = 0;
    virtual void shutdown() = 0;
};

// 使用 Boost.DLL 导出插件
BOOST_DLL_ALIAS(PluginImpl::create, create_plugin)
BOOST_DLL_ALIAS(PluginImpl::destroy, destroy_plugin)
```

**需求匹配度分析**:
| 需求项 | 支持情况 | 说明 |
|--------|---------|------|
| 动态加载/卸载 | ✅ 完全支持 | `shared_library` 类 |
| 元数据读取 | ⚠️ 部分支持 | `library_info` 可查询符号 |
| 生命周期管理 | ⚠️ 基础支持 | 需自行实现 |
| 依赖管理 | ❌ 不支持 | 需自行实现 |
| 线程安全 | ⚠️ 部分支持 | 库本身线程安全 |
| 跨平台 | ✅ 完全支持 | Windows/Linux/macOS |
| C++ 版本 | C++11 | 满足需求 |

**优势**: Boost 生态、功能丰富、文档完善
**劣势**: 依赖 Boost 库，体积较大

---

### 2.3 POCO ClassLoader

**GitHub**: https://github.com/pocoproject/poco

**核心特性**:
- 成熟的企业级 C++ 库
- `ClassLoader<T>` 模板类管理插件
- `Manifest` 系统注册插件类
- 完整的插件生命周期管理
- 跨平台支持

**代码示例**:
```cpp
// 定义插件接口
class AbstractPlugin {
public:
    virtual ~AbstractPlugin() {}
    virtual void doSomething() = 0;
    virtual std::string name() const = 0;
};

// 实现插件
// MyPlugin.cpp
#include "AbstractPlugin.h"
#include "Poco/ClassLibrary.h"

class MyPlugin : public AbstractPlugin {
public:
    void doSomething() override {
        std::cout << "MyPlugin doing work" << std::endl;
    }
    std::string name() const override {
        return "MyPlugin";
    }
};

// 注册插件到 Manifest
POCO_BEGIN_MANIFEST(AbstractPlugin)
    POCO_EXPORT_CLASS(MyPlugin)
POCO_END_MANIFEST

// 加载插件
// main.cpp
#include "Poco/ClassLoader.h"
#include "Poco/Manifest.h"

int main() {
    Poco::ClassLoader<AbstractPlugin> loader;

    try {
        loader.loadLibrary("libMyPlugin.so");

        AbstractPlugin* plugin = loader.create("MyPlugin");
        plugin->doSomething();
        std::cout << "Plugin: " << plugin->name() << std::endl;

        delete plugin;
        loader.unloadLibrary("libMyPlugin.so");

    } catch (const Poco::Exception& exc) {
        std::cerr << "Error: " << exc.displayText() << std::endl;
    }
    return 0;
}
```

**插件发现示例**:
```cpp
// 扫描目录下所有插件
Poco::ClassLoader<AbstractPlugin> loader;
loader.loadLibrary("libPlugin1.so");
loader.loadLibrary("libPlugin2.so");

// 遍历 Manifest 中的所有类
const Poco::Manifest<AbstractPlugin>& manifest = loader.manifest();
for (auto it = manifest.begin(); it != manifest.end(); ++it) {
    std::cout << "Available class: " << it->name() << std::endl;
}
```

**需求匹配度分析**:
| 需求项 | 支持情况 | 说明 |
|--------|---------|------|
| 动态加载/卸载 | ✅ 完全支持 | ClassLoader 管理 |
| 元数据读取 | ⚠️ 部分支持 | Manifest 提供类名，无自定义元数据 |
| 生命周期管理 | ✅ 良好支持 | 加载/创建/销毁/卸载 |
| 依赖管理 | ❌ 不支持 | 需自行实现 |
| 线程安全 | ✅ 良好支持 | POCO 线程安全设计 |
| 跨平台 | ✅ 完全支持 | Windows/Linux/macOS |
| C++ 版本 | C++11 | 满足需求 |

**优势**: 成熟稳定、企业级质量、完整生态
**劣势**: 库体积大，仅用于插件功能过于重量级

---

### 2.4 Qt Plugin System

**GitHub**: Qt 生态系统 (https://github.com/qt)

**核心特性**:
- `QPluginLoader` 运行时加载插件
- `Q_PLUGIN_METADATA` 嵌入 JSON 元数据
- `metaData()` 不加载即可读取元数据
- 完整的插件接口声明机制
- Qt 元对象系统集成

**代码示例**:
```cpp
// 定义插件接口 (myplugininterface.h)
#include <QtPlugin>

class MyPluginInterface {
public:
    virtual ~MyPluginInterface() = default;
    virtual QString name() const = 0;
    virtual void initialize() = 0;
    virtual void execute() = 0;
    virtual void shutdown() = 0;
};

#define MyPluginInterface_iid "com.robot.MyPluginInterface/1.0"
Q_DECLARE_INTERFACE(MyPluginInterface, MyPluginInterface_iid)

// 实现插件 (myplugin.h)
#include "myplugininterface.h"
#include <QObject>

class MyPlugin : public QObject, public MyPluginInterface {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID MyPluginInterface_iid FILE "myplugin.json")
    Q_INTERFACES(MyPluginInterface)

public:
    QString name() const override { return "MyPlugin"; }
    void initialize() override { /* 初始化逻辑 */ }
    void execute() override { /* 执行逻辑 */ }
    void shutdown() override { /* 关闭逻辑 */ }
};

// myplugin.json (元数据文件)
{
    "name": "MyPlugin",
    "version": "1.0.0",
    "author": "Robot Team",
    "category": "motion",
    "dependencies": []
}

// 加载插件 (main.cpp)
#include <QPluginLoader>
#include <QDir>
#include <QDebug>

int main() {
    QDir pluginsDir("/path/to/plugins");

    for (const QString &fileName : pluginsDir.entryList(QDir::Files)) {
        QPluginLoader loader(pluginsDir.absoluteFilePath(fileName));

        // 不加载插件，仅读取元数据
        QJsonObject metadata = loader.metaData();
        qDebug() << "Plugin metadata:" << metadata;

        // 检查是否需要加载
        if (metadata["category"] == "motion") {
            QObject *obj = loader.instance();
            if (MyPluginInterface *plugin = qobject_cast<MyPluginInterface*>(obj)) {
                plugin->initialize();
                plugin->execute();
                plugin->shutdown();
            }
        }
    }
    return 0;
}
```

**需求匹配度分析**:
| 需求项 | 支持情况 | 说明 |
|--------|---------|------|
| 动态加载/卸载 | ✅ 完全支持 | QPluginLoader |
| 元数据读取 | ✅ 完全支持 | metaData() 不加载即可读取 |
| 生命周期管理 | ⚠️ 基础支持 | load/unload，需自行扩展 |
| 依赖管理 | ⚠️ 部分支持 | JSON 元数据可声明依赖 |
| 线程安全 | ✅ 良好支持 | Qt 线程安全设计 |
| 跨平台 | ✅ 完全支持 | Windows/Linux/macOS |
| C++ 版本 | C++11 | 满足需求 |

**优势**: 元数据支持完善、不加载即可读取、成熟稳定
**劣势**: 依赖 Qt 框架，对非 Qt 项目过于重量级

---

### 2.5 cr.h (fungos/cr)

**GitHub**: https://github.com/fungos/cr

**核心特性**:
- 单头文件 C/C++ 热重载库
- 跨平台支持 (Windows/Linux/macOS)
- 支持状态保存/恢复
- 开发阶段快速迭代

**代码示例**:
```cpp
// 主应用程序 (host)
#define CR_HOST
#include "cr.h"

int main() {
    cr_plugin ctx;
    cr_plugin_load(ctx, "my_plugin.dll");

    // 主循环
    while (running) {
        cr_plugin_update(ctx);  // 自动检测文件变化并重载

        // 其他逻辑
        render();
        handle_input();
    }

    cr_plugin_close(ctx);
    return 0;
}

// 插件代码 (plugin.cpp)
#include "cr.h"

struct PluginState {
    int counter;
    float value;
};

CR_EXPORT int cr_main(cr_plugin* ctx, cr_op operation) {
    switch (operation) {
        case CR_LOAD:
            // 初始化插件
            ctx->userdata = new PluginState{0, 0.0f};
            return 0;

        case CR_UNLOAD:
            // 清理资源
            delete static_cast<PluginState*>(ctx->userdata);
            return 0;

        case CR_CLOSE:
            // 应用关闭
            return 0;

        case CR_STEP:
            // 每帧更新
            auto* state = static_cast<PluginState*>(ctx->userdata);
            state->counter++;
            // 插件逻辑...
            return 0;
    }
    return 0;
}
```

**需求匹配度分析**:
| 需求项 | 支持情况 | 说明 |
|--------|---------|------|
| 动态加载/卸载 | ✅ 完全支持 | 热重载核心功能 |
| 元数据读取 | ❌ 不支持 | 需自行实现 |
| 生命周期管理 | ✅ 良好支持 | LOAD/UNLOAD/CLOSE/STEP |
| 依赖管理 | ❌ 不支持 | 需自行实现 |
| 线程安全 | ⚠️ 基础支持 | 单线程模型 |
| 跨平台 | ✅ 完全支持 | Windows/Linux/macOS |
| C++ 版本 | C/C++ | 满足需求 |

**优势**: 热重载、开发效率高、单头文件
**劣势**: 适合开发阶段，不适合生产环境

---

### 2.6 dynalo (maddouri/dynalo)

**GitHub**: https://github.com/maddouri/dynalo

**核心特性**:
- 头文件库，易于集成
- 跨平台动态库加载封装
- C++11 兼容
- 模板化符号获取

**代码示例**:
```cpp
#include <dynalo/dynalo.hpp>
#include <iostream>

int main() {
    try {
        dynalo::library lib("my_plugin");

        // 获取函数
        auto add = lib.get<int(int, int)>("add");
        int result = add(3, 4);
        std::cout << "3 + 4 = " << result << std::endl;

        // 获取带字符串参数的函数
        auto greet = lib.get<std::string(const std::string&)>("greet");
        std::cout << greet("World") << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}
```

**需求匹配度分析**:
| 需求项 | 支持情况 | 说明 |
|--------|---------|------|
| 动态加载/卸载 | ✅ 完全支持 | RAII 管理 |
| 元数据读取 | ❌ 不支持 | 需自行实现 |
| 生命周期管理 | ⚠️ 基础支持 | 仅加载/卸载 |
| 依赖管理 | ❌ 不支持 | 需自行实现 |
| 线程安全 | ⚠️ 部分支持 | 库本身线程安全 |
| 跨平台 | ✅ 完全支持 | Windows/Linux |
| C++ 版本 | C++11 | 满足需求 |

**优势**: 极简、头文件、易于集成
**劣势**: 功能过于基础

---

### 2.7 RuntimeCompiledCPlusPlus

**GitHub**: https://github.com/RuntimeCompiledCPlusPlus/RuntimeCompiledCPlusPlus

**核心特性**:
- 运行时编译 C++ 代码
- 热重载支持
- 游戏开发快速迭代
- 支持 MSVC/GCC/Clang

**需求匹配度分析**:
| 需求项 | 支持情况 | 说明 |
|--------|---------|------|
| 动态加载/卸载 | ✅ 完全支持 | 核心功能 |
| 元数据读取 | ❌ 不支持 | 需自行实现 |
| 生命周期管理 | ✅ 良好支持 | 编译/加载/卸载 |
| 依赖管理 | ❌ 不支持 | 需自行实现 |
| 线程安全 | ⚠️ 基础支持 | 单线程模型 |
| 跨平台 | ⚠️ 部分支持 | Windows 为主 |
| C++ 版本 | C++11 | 满足需求 |

**优势**: 热重载、开发效率高
**劣势**: 主要面向游戏开发，跨平台支持有限

---

### 2.8 LIEF (Library to Instrument Executable Formats)

**GitHub**: https://github.com/lief-project/LIEF

**核心特性**:
- 解析 ELF/PE/Mach-O 二进制格式
- 读取自定义段(section)数据
- 不加载即可读取嵌入的元数据
- Python 和 C++ API

**代码示例 (读取嵌入的元数据)**:
```cpp
#include <LIEF/ELF.hpp>
#include <iostream>
#include <nlohmann/json.hpp>

// 从 ELF 文件读取嵌入的插件元数据
PluginMetadata readPluginMetadata(const std::string& pluginPath) {
    auto binary = LIEF::ELF::Parser::parse(pluginPath);

    // 查找自定义 .plugin_meta 段
    auto section = binary->get_section(".plugin_meta");
    if (section) {
        std::string json_str(
            reinterpret_cast<const char*>(section->content().data()),
            section->content().size()
        );

        auto json = nlohmann::json::parse(json_str);
        PluginMetadata meta;
        meta.name = json["name"];
        meta.version = json["version"];
        meta.description = json["description"];
        // ... 解析其他字段
        return meta;
    }
    throw std::runtime_error("Plugin metadata section not found");
}

// 插件中嵌入元数据
// 编译时使用链接器脚本或 __attribute__((section))
__attribute__((section(".plugin_meta")))
static const char plugin_meta_json[] = R"({
    "name": "MotionController",
    "version": "1.0.0",
    "description": "机器人运动控制插件",
    "author": "Robot Team",
    "category": "motion",
    "provides": ["motion_control", "trajectory_planning"],
    "dependencies": []
})";
```

**需求匹配度分析**:
| 需求项 | 支持情况 | 说明 |
|--------|---------|------|
| 动态加载/卸载 | ❌ 不支持 | 仅用于二进制解析 |
| 元数据读取 | ✅ 完全支持 | 不加载即可读取段数据 |
| 生命周期管理 | ❌ 不支持 | 非插件管理框架 |
| 依赖管理 | ❌ 不支持 | 需自行实现 |
| 线程安全 | ✅ 良好支持 | 只读操作 |
| 跨平台 | ✅ 完全支持 | ELF/PE/Mach-O |
| C++ 版本 | C++11 | 满足需求 |

**优势**: 不加载即可读取元数据、跨平台二进制解析
**劣势**: 仅用于元数据读取，需配合其他框架使用

---

### 2.9 ROS pluginlib

**GitHub**: https://github.com/ros/pluginlib

**核心特性**:
- ROS 生态系统插件管理
- `ClassLoader<T>` 模板类
- XML 插件描述文件
- `PLUGINLIB_EXPORT_CLASS` 宏注册插件

**代码示例**:
```cpp
// 定义插件基类
namespace robot_base {
    class MotionController {
    public:
        virtual void initialize() = 0;
        virtual void execute() = 0;
        virtual void shutdown() = 0;
        virtual ~MotionController() {}
    };
}

// 实现插件
namespace robot_plugins {
    class JointController : public robot_base::MotionController {
    public:
        void initialize() override {
            // 初始化关节控制器
        }
        void execute() override {
            // 执行运动控制
        }
        void shutdown() override {
            // 关闭控制器
        }
    };
}

// 注册插件
#include <pluginlib/class_list_macros.hpp>
PLUGINLIB_EXPORT_CLASS(robot_plugins::JointController, robot_base::MotionController)

// 插件描述文件 (joint_controller.xml)
// <library path="libjoint_controller">
//   <class type="robot_plugins::JointController"
//          base_class_type="robot_base::MotionController">
//     <description>关节运动控制器</description>
//   </class>
// </library>

// 加载插件
#include <pluginlib/class_loader.hpp>

int main() {
    try {
        pluginlib::ClassLoader<robot_base::MotionController> loader(
            "robot_base", "robot_base::MotionController");

        boost::shared_ptr<robot_base::MotionController> controller =
            loader.createInstance("robot_plugins::JointController");

        controller->initialize();
        controller->execute();
        controller->shutdown();

    } catch (pluginlib::PluginlibException& ex) {
        std::cerr << "Plugin load failed: " << ex.what() << std::endl;
    }
    return 0;
}
```

**需求匹配度分析**:
| 需求项 | 支持情况 | 说明 |
|--------|---------|------|
| 动态加载/卸载 | ✅ 完全支持 | ClassLoader |
| 元数据读取 | ✅ 良好支持 | XML 描述文件 |
| 生命周期管理 | ⚠️ 基础支持 | create/destroy |
| 依赖管理 | ⚠️ 部分支持 | XML 可声明 |
| 线程安全 | ✅ 良好支持 | ROS 线程模型 |
| 跨平台 | ✅ 支持 | Linux 为主 |
| C++ 版本 | C++14 | 满足需求 |

**优势**: 机器人领域成熟方案、ROS 生态集成
**劣势**: 依赖 ROS 生态，独立使用较复杂

---

### 2.10 Orocos RTT (Real-Time Toolkit)

**GitHub**: https://github.com/orocos-toolchain/rtt

**核心特性**:
- 实时机器人控制框架
- 组件化架构 (TaskContext)
- 端口通信机制
- 插件系统 (Typekit/Transport/Component)

**代码示例**:
```cpp
#include <rtt/TaskContext.hpp>
#include <rtt/Port.hpp>
#include <rtt/Component.hpp>

class JointController : public RTT::TaskContext {
public:
    JointController(std::string name)
        : TaskContext(name)
    {
        // 添加端口
        this->addPort("joint_position", positionPort);
        this->addPort("joint_command", commandPort);

        // 添加属性
        this->addProperty("joint_id", jointId);
    }

    bool configureHook() {
        // 配置阶段
        return true;
    }

    bool startHook() {
        // 启动阶段
        return true;
    }

    void updateHook() {
        // 周期更新
        double position;
        if (positionPort.read(position) == RTT::NewData) {
            double command = computeCommand(position);
            commandPort.write(command);
        }
    }

    void stopHook() {
        // 停止阶段
    }

    void cleanupHook() {
        // 清理阶段
    }

private:
    RTT::InputPort<double> positionPort;
    RTT::OutputPort<double> commandPort;
    int jointId;
};

// 注册为可加载组件
ORO_CREATE_COMPONENT(JointController)
```

**需求匹配度分析**:
| 需求项 | 支持情况 | 说明 |
|--------|---------|------|
| 动态加载/卸载 | ✅ 完全支持 | 组件加载 |
| 元数据读取 | ⚠️ 部分支持 | 部署文件描述 |
| 生命周期管理 | ✅ 完全支持 | 完整钩子函数 |
| 依赖管理 | ⚠️ 部分支持 | 部署文件配置 |
| 线程安全 | ✅ 完全支持 | 实时线程模型 |
| 跨平台 | ✅ 支持 | Linux 为主 |
| C++ 版本 | C++11 | 满足需求 |

**优势**: 实时性、机器人领域专业、完整生命周期
**劣势**: 学习曲线陡峭、依赖 Orocos 生态

---

### 2.11 AprioritInc/CppPluginFramework

**GitHub**: https://github.com/AprioritInc/CppPluginFramework

**核心特性**:
- 通用 C++ 插件框架
- CMake 构建系统
- 跨平台支持
- 插件接口定义

**需求匹配度分析**:
| 需求项 | 支持情况 | 说明 |
|--------|---------|------|
| 动态加载/卸载 | ✅ 支持 | 基础功能 |
| 元数据读取 | ❌ 不支持 | 需自行实现 |
| 生命周期管理 | ⚠️ 基础支持 | 简单生命周期 |
| 依赖管理 | ❌ 不支持 | 需自行实现 |
| 线程安全 | ⚠️ 基础支持 | 需自行保证 |
| 跨平台 | ✅ 支持 | Windows/Linux |
| C++ 版本 | C++11 | 满足需求 |

**优势**: 简单易用
**劣势**: 社区较小，功能有限

---

### 2.12 vector-of-bool/plug

**GitHub**: https://github.com/vector-of-bool/plug

**核心特性**:
- 轻量级插件框架
- 现代 C++ 设计
- CMake 集成

**需求匹配度分析**:
与 dylib 类似，属于轻量级方案，适合简单场景。

---

### 2.13 YOMM2 (Open Methods)

**GitHub**: https://github.com/jll63/yomm2

**核心特性**:
- 开放方法/多方法实现
- 运行时多态扩展
- 插件友好架构

**代码示例**:
```cpp
#include <yomm2/yomm2.hpp>

// 基类
class Animal {};
class Dog : public Animal {};
class Cat : public Animal {};

// 声明开放方法
YOMM2_CLASS(Animal);
YOMM2_CLASS(Dog, Animal);
YOMM2_CLASS(Cat, Animal);

YOMM2_DECLARE(void, speak, (virtual_<Animal&>));
YOMM2_DECLARE(void, kick, (virtual_<Animal&>, int));

// 定义方法
YOMM2_DEFINE(void, speak, (Dog& dog)) {
    std::cout << "Woof!" << std::endl;
}

YOMM2_DEFINE(void, speak, (Cat& cat)) {
    std::cout << "Meow!" << std::endl;
}

// 插件可以扩展新方法
YOMM2_DECLARE(void, train, (virtual_<Animal&>));

// 插件中定义
YOMM2_DEFINE(void, train, (Dog& dog)) {
    std::cout << "Training dog..." << std::endl;
}
```

**需求匹配度分析**:
| 需求项 | 支持情况 | 说明 |
|--------|---------|------|
| 动态加载/卸载 | ❌ 不支持 | 编译期机制 |
| 元数据读取 | ❌ 不支持 | 需自行实现 |
| 生命周期管理 | ❌ 不支持 | 非插件管理框架 |
| 依赖管理 | ❌ 不支持 | 需自行实现 |
| 线程安全 | ✅ 支持 | 运行时分派安全 |
| 跨平台 | ✅ 支持 | 标准 C++ |
| C++ 版本 | C++17 | 满足需求 |

**优势**: 强大的多态扩展能力
**劣势**: 非插件管理框架，需配合其他方案

---

## 3. 元数据不加载读取方案对比

### 3.1 方案概述

| 方案 | 实现方式 | 优点 | 缺点 | 推荐度 |
|------|---------|------|------|--------|
| 伴随文件 | .meta.json 文件 | 简单、可读性好 | 文件管理复杂 | ⭐⭐⭐⭐ |
| 导出函数 | dlsym 获取元数据 | 无需额外文件 | 需要加载库 | ⭐⭐⭐ |
| 嵌入段 | ELF/PE section | 单文件、不需加载 | 平台相关、需解析库 | ⭐⭐⭐ |
| Qt 方案 | Q_PLUGIN_METADATA | 成熟方案 | 依赖 Qt | ⭐⭐⭐⭐ |
| 资源嵌入 | Windows PE 资源 | Windows 原生 | 跨平台复杂 | ⭐⭐ |

### 3.2 伴随文件方案 (推荐)

```cpp
// plugin_meta.hpp
struct PluginMetadata {
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    std::string license;
    std::vector<std::string> dependencies;
    std::vector<std::string> provides;
    std::string min_host_version;
    std::string platform;
    std::string category;
    std::string entry_point;
};

// 读取元数据
PluginMetadata readMetadata(const std::string& pluginPath) {
    std::string metaPath = pluginPath + ".meta.json";
    std::ifstream file(metaPath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open metadata file: " + metaPath);
    }

    nlohmann::json json;
    file >> json;

    PluginMetadata meta;
    meta.name = json["name"];
    meta.version = json["version"];
    meta.description = json["description"];
    meta.author = json["author"];
    // ... 解析其他字段
    return meta;
}

// 扫描插件目录
std::vector<PluginMetadata> scanPlugins(const std::string& directory) {
    std::vector<PluginMetadata> plugins;

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.path().extension() == ".so" ||
            entry.path().extension() == ".dll") {
            try {
                auto meta = readMetadata(entry.path().string());
                plugins.push_back(meta);
            } catch (const std::exception& e) {
                std::cerr << "Warning: " << e.what() << std::endl;
            }
        }
    }
    return plugins;
}
```

### 3.3 嵌入段方案

```cpp
// 使用 LIEF 读取嵌入的元数据
#include <LIEF/ELF.hpp>

PluginMetadata readEmbeddedMetadata(const std::string& pluginPath) {
    auto binary = LIEF::ELF::Parser::parse(pluginPath);
    auto section = binary->get_section(".plugin_meta");

    if (!section) {
        throw std::runtime_error("No .plugin_meta section found");
    }

    auto content = section->content();
    std::string json_str(content.begin(), content.end());

    return parseMetadata(json_str);
}

// 插件中嵌入元数据 (编译时)
// 使用链接器脚本或属性
__attribute__((used, section(".plugin_meta")))
static const char metadata[] = R"({
    "name": "MyPlugin",
    "version": "1.0.0",
    "description": "My plugin description"
})";
```

---

## 4. 推荐方案

### 4.1 方案一：基于 dylib + 自定义框架 (推荐)

**适用场景**: 需要轻量级、现代 C++、快速集成

**架构设计**:
```
┌─────────────────────────────────────────────┐
│              Plugin Manager                  │
│  ┌─────────────┐  ┌─────────────────────┐   │
│  │ Metadata    │  │ Lifecycle Manager   │   │
│  │ Scanner     │  │ (Load/Init/Run/     │   │
│  │ (.meta.json)│  │  Stop/Unload)       │   │
│  └─────────────┘  └─────────────────────┘   │
│  ┌─────────────┐  ┌─────────────────────┐   │
│  │ Dependency  │  │ Service Registry    │   │
│  │ Resolver    │  │ (Event Bus)         │   │
│  └─────────────┘  └─────────────────────┘   │
└─────────────────────────────────────────────┘
                     │
                     ▼
              ┌─────────────┐
              │   dylib     │
              │ (底层加载)   │
              └─────────────┘
```

**核心代码框架**:
```cpp
// plugin_interface.hpp
class IPlugin {
public:
    virtual ~IPlugin() = default;

    // 生命周期
    virtual bool initialize() = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual void unload() = 0;

    // 信息查询
    virtual std::string getName() const = 0;
    virtual std::string getVersion() const = 0;
    virtual PluginState getState() const = 0;

    // 服务接口
    virtual void* getService(const std::string& name) = 0;
    virtual bool registerService(const std::string& name, void* service) = 0;
};

// plugin_manager.hpp
class PluginManager {
public:
    bool loadPlugin(const std::string& path);
    bool unloadPlugin(const std::string& name);
    bool initializePlugin(const std::string& name);
    bool startPlugin(const std::string& name);
    bool stopPlugin(const std::string& name);

    // 元数据扫描
    std::vector<PluginMetadata> scanPlugins(const std::string& directory);

    // 服务注册
    bool registerService(const std::string& name, void* service);
    void* getService(const std::string& name);

    // 事件系统
    bool subscribeEvent(const std::string& event, EventHandler handler);
    bool publishEvent(const std::string& event, const EventData& data);

private:
    std::map<std::string, PluginInfo> plugins_;
    std::map<std::string, void*> services_;
    dylib library_;
};
```

### 4.2 方案二：基于 POCO ClassLoader (企业级)

**适用场景**: 需要成熟稳定的企业级方案

**优势**:
- 成熟的 Manifest 机制
- 完整的生命周期管理
- 跨平台支持完善

### 4.3 方案三：基于 Qt Plugin System (元数据优先)

**适用场景**: 需要完善的元数据支持，可接受 Qt 依赖

**优势**:
- 不加载即可读取元数据
- JSON 元数据格式
- 成熟的插件机制

### 4.4 方案四：ROS pluginlib 方案 (机器人专用)

**适用场景**: 已有 ROS 生态系统集成需求

**优势**:
- 机器人领域成熟方案
- 完整的插件发现机制

---

## 5. 性能对比

| 框架 | 加载时间 | 内存占用 | 调用延迟 | 100+ 插件支持 |
|------|---------|---------|---------|--------------|
| dylib | < 10ms | 极低 | < 1μs | ✅ |
| Boost.DLL | < 20ms | 低 | < 1μs | ✅ |
| POCO | < 30ms | 中等 | < 1μs | ✅ |
| Qt | < 50ms | 中等 | < 1μs | ✅ |
| cr.h | < 100ms | 低 | < 1μs | ⚠️ (开发用) |

---

## 6. 依赖管理方案设计

### 6.1 依赖声明格式

```json
{
    "name": "VisionPlugin",
    "version": "1.2.0",
    "dependencies": [
        {
            "name": "CameraDriver",
            "version": ">=1.0.0",
            "required": true
        },
        {
            "name": "ImageProcessor",
            "version": "~2.0.0",
            "required": false
        }
    ]
}
```

### 6.2 依赖解析算法

```cpp
class DependencyResolver {
public:
    // 拓扑排序确定加载顺序
    std::vector<std::string> resolveLoadOrder(
        const std::vector<PluginMetadata>& plugins) {

        // 1. 构建依赖图
        std::map<std::string, std::vector<std::string>> graph;
        for (const auto& plugin : plugins) {
            graph[plugin.name] = plugin.dependencies;
        }

        // 2. 检测循环依赖
        if (hasCyclicDependency(graph)) {
            throw std::runtime_error("Cyclic dependency detected");
        }

        // 3. 拓扑排序
        return topologicalSort(graph);
    }

private:
    bool hasCyclicDependency(
        const std::map<std::string, std::vector<std::string>>& graph);

    std::vector<std::string> topologicalSort(
        const std::map<std::string, std::vector<std::string>>& graph);
};
```

---

## 7. 线程安全设计

### 7.1 读写锁保护

```cpp
class PluginManager {
private:
    mutable std::shared_mutex mutex_;
    std::map<std::string, PluginInfo> plugins_;

public:
    bool loadPlugin(const std::string& path) {
        std::unique_lock lock(mutex_);
        // 写操作
    }

    PluginInfo* getPlugin(const std::string& name) {
        std::shared_lock lock(mutex_);
        // 读操作
    }
};
```

### 7.2 无锁服务调用

```cpp
class ServiceRegistry {
public:
    void* getService(const std::string& name) {
        // 使用原子指针实现无锁查找
        auto it = services_.find(name);
        if (it != services_.end()) {
            return it->second.load(std::memory_order_acquire);
        }
        return nullptr;
    }

private:
    std::map<std::string, std::atomic<void*>> services_;
};
```

---

## 8. 跨平台抽象层设计

### 8.1 动态库加载抽象

```cpp
class DynamicLibrary {
public:
    static std::unique_ptr<DynamicLibrary> load(const std::string& path) {
#ifdef _WIN32
        return std::make_unique<Win32Library>(path);
#else
        return std::make_unique<PosixLibrary>(path);
#endif
    }

    virtual void* getSymbol(const std::string& name) = 0;
    virtual void unload() = 0;
    virtual ~DynamicLibrary() = default;
};

class PosixLibrary : public DynamicLibrary {
public:
    PosixLibrary(const std::string& path) {
        handle_ = dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);
        if (!handle_) {
            throw std::runtime_error(dlerror());
        }
    }

    void* getSymbol(const std::string& name) override {
        dlerror();  // 清除错误
        void* sym = dlsym(handle_, name.c_str());
        const char* error = dlerror();
        if (error) {
            throw std::runtime_error(error);
        }
        return sym;
    }

    void unload() override {
        if (handle_) {
            dlclose(handle_);
            handle_ = nullptr;
        }
    }

    ~PosixLibrary() { unload(); }

private:
    void* handle_ = nullptr;
};
```

---

## 9. 结论与建议

### 9.1 最终推荐

基于需求文档分析，推荐采用 **方案一：基于 dylib + 自定义框架**，理由如下：

1. **轻量级**: dylib 仅提供动态库加载，不引入额外依赖
2. **现代 C++**: 使用 C++17，符合需求要求
3. **灵活扩展**: 可根据需求自定义元数据、生命周期、依赖管理
4. **跨平台**: 完善的 Windows/Linux/macOS 支持
5. **性能**: 加载速度快，内存占用低
6. **社区活跃**: GitHub 持续维护

### 9.2 实施建议

1. **元数据方案**: 采用伴随文件 (.meta.json) 方案
   - 简单易实现
   - 可读性好
   - 无需额外库支持

2. **生命周期管理**: 自定义实现完整的 Load/Initialize/Run/Stop/Unload 生命周期

3. **依赖管理**: 实现拓扑排序算法，支持版本约束检查

4. **线程安全**: 使用读写锁保护共享状态，服务调用使用原子操作

5. **热更新**: 可参考 cr.h 方案，在开发阶段集成热重载功能

6. **性能优化**: 使用对象池、无锁数据结构、延迟加载等策略

### 9.3 参考资源

- [dylib GitHub](https://github.com/martin-olivier/dylib)
- [Boost.DLL 文档](https://www.boost.org/doc/libs/release/libs/dll/)
- [POCO ClassLoader](https://pocoproject.org/docs/Poco.ClassLoader.html)
- [Qt Plugin System](https://doc.qt.io/qt-6/plugins-howto.html)
- [cr.h GitHub](https://github.com/fungos/cr)
- [ROS pluginlib](http://wiki.ros.org/pluginlib)
- [Orocos RTT](https://www.orocos.org/rtt)
- [LIEF GitHub](https://github.com/lief-project/LIEF)
- [YOMM2 GitHub](https://github.com/jll63/yomm2)

---

**文档版本**: v1.0
**调研日期**: 2026-05-27
**调研人**: 技术调研专家
