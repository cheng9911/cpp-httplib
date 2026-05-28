# 快速使用指南

## 环境要求

- C++17 编译器 (GCC 8+)
- CMake 3.14+
- Python 3 (用于启动 Web UI 的 HTTP 服务)
- Linux 系统

## 首次构建

```bash
cd /home/sun/Documents/GitHub/cpp-httplib/robot_plugin_framework
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

构建完成后，所有可执行文件和插件在 `build/examples/` 下。

## 启动步骤

### 第 1 步：启动调试服务器

```bash
cd /home/sun/Documents/GitHub/cpp-httplib/robot_plugin_framework/build/examples/debug_server
./debug_server --port 8080
```

启动成功会看到：

```
=== Robot Debug Server ===
Plugin directory: ./plugins
Server port: 8080

Scanning plugins...
Found 3 plugins
Loading mujoco_hardware...
  -> Started
  -> Simulation started
...
[DebugAPI] Starting server on port 8080
```

### 第 2 步：启动 Web UI（新终端）

```bash
cd /home/sun/Documents/GitHub/cpp-httplib/robot_plugin_framework/examples/debug_server/web
python3 -m http.server 8888
```

### 第 3 步：浏览器打开

```
http://localhost:8888/?port=8080
```

端口说明：
- `8080` -- debug_server 的 API 端口（第 1 步设置的）
- `8888` -- Web UI 的 HTTP 端口（第 2 步设置的）
- URL 中的 `?port=8080` 告诉 Web UI 去哪个端口调用 API

## 验证是否正常

```bash
# 测试 API 连通性
curl http://localhost:8080/api/robot/state

# 正常返回 JSON，is_moving 为 true 表示仿真已运行
```

## 命令行参数

```bash
./debug_server [选项]

  --plugin-dir <dir>   插件目录（默认: ./plugins）
  --port <port>        API 端口（默认: 8080）
  --help               显示帮助
```

## 停止程序

在 debug_server 的终端按 `Ctrl+C`，服务器会优雅退出。

## Web UI 功能一览

| 区域 | 功能 |
|------|------|
| 插件管理 (Plugin Management) | 扫描、加载、启动插件 |
| 仿真控制 (Simulation Control) | 自动运行、停止、重置、步进 |
| 关节控制 (Joint Control) | 6 个滑条控制关节位置，点 Set 生效 |
| 运动规划 (Motion Planning) | 预设动作和自定义轨迹执行 |
| 关节图表 (Joint Chart) | 实时位置曲线 |
| 力矩图表 (Effort Chart) | 实时力矩曲线 |
| 传感器 (Sensor Data) | 力/加速度数据 |
| 日志 (Log) | 操作日志和错误信息 |

## 预设动作说明

| 预设 | 说明 | 时长 |
|------|------|------|
| Home | 所有关节归零 | 2s |
| Ready | 准备姿态 | 2s |
| Wave | 挥手动作（含中间点） | 3s |
| Point | 指向动作 | 2s |
| Fold | 折叠收纳 | 3s |

## 常见问题

### Q: 端口被占用

```bash
# 查看占用端口的进程
lsof -i :8080

# 杀掉旧进程
kill <PID>

# 或换个端口
./debug_server --port 8081
```

### Q: Web UI 显示 Disconnected

- 检查 debug_server 是否在运行
- 检查 URL 中的 `?port=` 是否和 debug_server 的端口一致
- 确认 Web UI 的 HTTP 服务（python3 http.server）正在运行

### Q: Plan & Execute 提示 Failed to fetch

- 确认浏览器是通过 `http://localhost:8888` 打开的（不是直接打开 `file://`）
- `file://` 协议会触发 CORS 限制，必须通过 HTTP 服务访问

### Q: 重新编译后需要重启

修改代码后重新构建，然后重启 debug_server：

```bash
# 重新编译
cd /home/sun/Documents/GitHub/cpp-httplib/robot_plugin_framework/build
cmake --build . -j$(nproc)

# 重启（先 Ctrl+C 停掉旧的，再重新启动）
cd examples/debug_server
./debug_server --port 8080
```

### Q: Web UI 滑条 Set 后值没变化

滑条显示格式为 `T:目标值 C:当前值`，Set 按钮设置的是目标值，PD 控制器会让关节逐步跟踪到目标。观察 C 值会逐渐趋近 T 值。
