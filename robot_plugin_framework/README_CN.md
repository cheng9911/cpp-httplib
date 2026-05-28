# Robot Plugin Framework (RPF)

轻量级 C++17 机器人插件框架。支持在运行时动态加载硬件驱动、运动控制、视觉检测等模块，内置 REST API 服务器和 Web UI 实时调试界面。

[English](README.md) | 中文

## 特性

- **动态插件加载** -- 运行时加载/卸载 `.so`/`.dll` 插件，无需重启宿主程序
- **生命周期管理** -- 严格状态机：`Unloaded -> Loaded -> Initialized -> Running -> Stopped`
- **服务定位器** -- 通过 `ServiceRegistry` 实现类型安全的插件间通信
- **事件总线** -- 发布/订阅消息机制，带异常隔离
- **依赖解析** -- 拓扑排序 + 环检测，确保加载顺序正确
- **硬件抽象** -- 统一的 `Hardware` 和 `Simulator` 接口
- **调试服务器** -- REST API (cpp-httplib) + Web UI (Chart.js)，实时监控关节和传感器
- **跨平台** -- 通过 `PLUGIN_API` 宏支持 Linux 和 Windows

## 架构

```
+------------------+     +-----------------+     +------------------+
|   宿主程序       |     |  PluginManager  |     |  插件 (.so)      |
|                  |---->|                 |---->|                  |
|  main()          |     |  扫描/加载/     |     |  IPlugin         |
|  创建插件        |     |  启动/停止      |     |  + Hardware      |
|  使用服务        |     |                 |     |  + MotionCtrl    |
+------------------+     +--------+--------+     |  + Vision        |
                                  |              +------------------+
                                  |
                    +-------------+-------------+
                    |             |             |
              +-----+-----+ +----+----+ +------+------+
              | EventBus  | | Service | | Dependency  |
              | 发布/订阅 | | Registry| | Resolver    |
              +-----------+ +---------+ +-------------+
```

### 核心模块

| 模块 | 文件 | 说明 |
|------|------|------|
| IPlugin | `plugin_interface.h` | 基础接口 + 生命周期方法 + `RPF_PLUGIN_EXPORT` 导出宏 |
| PluginManager | `plugin_manager.h/cpp` | 插件编排器，管理加载、状态转换和查询 |
| PluginMetadata | `plugin_metadata.h` | 数据模型 + JSON 序列化，对应 `.meta.json` 伴生文件 |
| EventBus | `event_bus.h` | Header-only 发布/订阅，每个 handler 独立异常隔离 |
| ServiceRegistry | `service_registry.h` | Header-only 类型安全服务定位器，基于 `std::type_index` |
| DependencyResolver | `dependency_resolver.h/cpp` | Kahn 拓扑排序 + DFS 环检测 |
| Hardware Interface | `hardware_interface.h` | 抽象 `Hardware` 和 `Simulator` 接口 |
| Motion Controller | `motion_controller.h` | 抽象 `MotionController` 和 `TrajectoryPlanner` 接口 |
| Vision Detector | `vision_detector.h` | 抽象 `VisionDetector` 和 `PoseEstimator` 接口 |

## 目录结构

```
robot_plugin_framework/
├── include/rpf/              # 公共头文件
├── src/                      # 核心库源码
├── examples/
│   ├── host_app/             # CLI 演示，完整插件生命周期
│   ├── mujoco_hardware_plugin/   # 6 关节机器人仿真 (PD 控制)
│   ├── motion_control_plugin/    # 关节运动 + 轨迹规划
│   ├── vision_plugin/            # 物体检测 + 位姿估计
│   └── debug_server/         # REST API 服务器 + Web UI
│       ├── debug_api.cpp     # 20+ REST 端点
│       ├── main.cpp          # 服务器入口
│       └── web/index.html    # 单页 Web 控制台
├── tests/                    # GTest 单元测试 (205 个用例)
├── third_party/              # FetchContent: dylib, nlohmann/json
└── CMakeLists.txt
```

## 环境要求

- C++17 编译器 (GCC 8+, Clang 7+, MSVC 2019+)
- CMake 3.14+
- Linux 或 Windows

第三方依赖通过 CMake FetchContent 自动拉取：
- [dylib](https://github.com/martin-olivier/dylib) v2.2.1 -- 跨平台动态库加载
- [nlohmann/json](https://github.com/nlohmann/json) v3.11.3 -- JSON 解析
- [Google Test](https://github.com/google/googletest) v1.14.0 -- 单元测试 (可选)

## 构建

```bash
cd robot_plugin_framework
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

构建选项：

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `RPF_BUILD_EXAMPLES` | ON | 构建示例插件和宿主程序 |
| `RPF_BUILD_TESTS` | ON | 构建单元测试 |
| `RPF_SHARED` | ON | 构建为动态库 (OFF 则为静态库) |

## 快速开始

### 1. 启动调试服务器

```bash
cd build/examples/debug_server
./debug_server --port 8080
```

服务器会自动加载 `./plugins/` 下的所有插件并启动仿真。

### 2. 打开 Web UI

启动 HTTP 服务后通过浏览器访问：

```bash
cd examples/debug_server/web
python3 -m http.server 8888
# 浏览器打开: http://localhost:8888/?port=8080
```

Web UI 功能：
- **插件管理** -- 扫描、加载、启动插件
- **关节控制** -- 6 个滑条，实时位置跟踪 (T: 目标值, C: 当前值)
- **运动规划** -- 预设动作 (Home/Ready/Wave/Point/Fold) 和自定义轨迹执行
- **仿真控制** -- 自动运行、停止、重置、步进 (x1/x5/x10/x50)
- **图表** -- 关节位置和力矩实时曲线 (Chart.js, 10Hz 刷新)
- **传感器** -- 力、力矩、IMU 数据显示

### 3. 运行 CLI 演示

```bash
cd build/examples/host_app
./host_app
```

## 编写插件

### 最小示例

```cpp
#include "rpf/plugin_interface.h"

class MyPlugin : public rpf::IPlugin {
public:
    bool initialize() override {
        // 初始化资源
        state_ = rpf::PluginState::Initialized;
        return true;
    }

    bool start() override {
        state_ = rpf::PluginState::Running;
        return true;
    }

    bool stop() override {
        state_ = rpf::PluginState::Stopped;
        return true;
    }

    void unload() override {
        // 释放资源
        state_ = rpf::PluginState::Unloaded;
    }

    rpf::PluginState getState() const override { return state_; }
    std::string getName() const override { return "my_plugin"; }
    std::string getVersion() const override { return "1.0.0"; }

private:
    rpf::PluginState state_ = rpf::PluginState::Unloaded;
};

RPF_PLUGIN_EXPORT(MyPlugin)
```

### 元数据文件

在 `.so` 旁创建 `my_plugin.meta.json`：

```json
{
    "name": "my_plugin",
    "version": "1.0.0",
    "description": "我的自定义插件",
    "author": "开发者",
    "category": "custom",
    "provides": ["MyService"],
    "dependencies": [],
    "entry_point": "libmy_plugin.so"
}
```

### 暴露服务

重写 `getService()` 向其他插件提供接口：

```cpp
class MyPlugin : public rpf::IPlugin, public MyService {
public:
    void* getService(const std::string& name) override {
        if (name == "MyService") return static_cast<MyService*>(this);
        return nullptr;
    }
    // ... 实现 MyService 方法
};
```

### 实现硬件接口

对于机器人硬件插件，实现 `rpf::Hardware` 接口：

```cpp
#include "rpf/hardware_interface.h"

class MyHardwarePlugin : public rpf::IPlugin, public rpf::Hardware {
public:
    void* getService(const std::string& name) override {
        if (name == "Hardware") return static_cast<rpf::Hardware*>(this);
        return nullptr;
    }

    rpf::RobotState getState() override { /* ... */ }
    bool setJointPosition(const std::string& name, double pos) override { /* ... */ }
    bool startSimulation() override { /* ... */ }
    bool stepSimulation(double dt) override { /* ... */ }
    // ... 其他 Hardware 方法
};
```

## REST API 参考

所有端点返回 JSON。成功响应包含 `"success": true`，错误响应包含 `"error": "<message>"`。

### 插件管理

| 方法 | 端点 | 说明 |
|------|------|------|
| GET | `/api/plugins/scan` | 扫描可用插件 |
| GET | `/api/plugins/loaded` | 获取已加载插件列表 |
| GET | `/api/plugins/{name}/state` | 获取插件状态 |
| GET | `/api/plugins/{name}/metadata` | 获取插件元数据 |
| POST | `/api/plugins/{name}/load` | 加载插件 |
| POST | `/api/plugins/{name}/unload` | 卸载插件 |
| POST | `/api/plugins/{name}/start` | 启动插件 |
| POST | `/api/plugins/{name}/stop` | 停止插件 |

### 硬件接口

| 方法 | 端点 | 说明 |
|------|------|------|
| GET | `/api/robot/state` | 获取所有关节位置、速度、力矩 |
| GET | `/api/robot/sensors` | 获取传感器数据 (力/力矩/IMU) |
| GET | `/api/robot/joints/{name}/position?value=X` | 设置单个关节位置 |
| GET | `/api/robot/joints/positions?joint1=X&joint2=Y&...` | 批量设置关节位置 |

### 仿真控制

| 方法 | 端点 | 说明 |
|------|------|------|
| POST | `/api/simulation/start` | 启动自动步进仿真 |
| POST | `/api/simulation/stop` | 停止仿真 |
| POST | `/api/simulation/reset` | 重置所有关节到零位 |
| POST | `/api/simulation/step` | 步进仿真 (body: `{"dt": 0.1}`) |

### 运动规划

| 方法 | 端点 | 说明 |
|------|------|------|
| GET | `/api/motion/plan?target=X,Y,Z,...&duration=T` | 规划并执行轨迹 (6 个值) |
| POST | `/api/motion/preset/{name}` | 执行预设动作: `home`/`ready`/`wave`/`point`/`fold` |
| POST | `/api/motion/stop` | 停止当前运动 |
| GET | `/api/motion/presets` | 获取预设动作列表及目标值 |
| GET | `/api/motion/status` | 获取运动执行状态和进度 |

### 服务和事件

| 方法 | 端点 | 说明 |
|------|------|------|
| GET | `/api/services` | 列出已注册服务 |
| POST | `/api/events/publish` | 发布事件 (body: `{"event": "name", "data": {...}}`) |

## 运动规划细节

运动规划器使用 **smoothstep 插值** 在航点之间生成平滑轨迹：

```
smoothstep(t) = t^2 * (3 - 2t)    其中 t 在 [0, 1] 范围内
```

两层控制架构：

1. **运动线程** (20Hz) -- 插值轨迹航点，设置关节目标
2. **仿真线程** (100Hz) -- PD 控制器跟踪目标，参数 `Kp=2.0, Kd=2.0`

PD 控制器计算公式：
```
力矩 = Kp * (目标位置 - 当前位置) - Kd * 速度
速度 += 力矩 * dt
位置 += 速度 * dt
```

## 示例插件

| 插件 | 分类 | 提供接口 | 说明 |
|------|------|----------|------|
| `mujoco_hardware` | hardware | Hardware, Simulator | 6 关节机械臂，PD 控制，力/力矩/IMU 传感器 |
| `motion_control` | motion | MotionController, TrajectoryPlanner | 关节位置控制 + 线性插值轨迹 |
| `vision` | vision | VisionDetector, PoseEstimator | 模拟物体检测和位姿估计 |

## 命令行参数

```
./debug_server [选项]
  --plugin-dir <dir>   插件目录 (默认: ./plugins)
  --port <port>        服务器端口 (默认: 8080)
  --help               显示帮助
```

## 测试

```bash
cd build
ctest --output-on-failure
```

测试套件：205 个单元测试，覆盖元数据、依赖解析、事件总线、服务注册、插件管理等模块。

## 线程安全

- `PluginManager`、`EventBus`、`ServiceRegistry` 使用 `std::shared_mutex` 实现并发读
- 硬件插件使用 `std::mutex` 保护关节状态
- 运动执行使用 `std::atomic` 标志位实现停止信号

## 许可证

各插件的许可信息见对应的元数据文件。
