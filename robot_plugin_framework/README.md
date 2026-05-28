# Robot Plugin Framework (RPF)

English | [中文](README_CN.md)

A lightweight, modular C++17 plugin framework for robotics applications. Dynamically load hardware drivers, motion controllers, and vision modules at runtime via shared libraries, with a built-in REST API server and Web UI for real-time debugging.

## Features

- **Dynamic Plugin Loading** -- Load/unload `.so`/`.dll` plugins at runtime without restarting the host
- **Lifecycle Management** -- Strict state machine: `Unloaded -> Loaded -> Initialized -> Running -> Stopped`
- **Service Locator** -- Type-safe inter-plugin communication via `ServiceRegistry`
- **Event Bus** -- Publish/subscribe messaging with exception isolation
- **Dependency Resolution** -- Topological sort with cycle detection for correct load ordering
- **Hardware Abstraction** -- Unified `Hardware` and `Simulator` interfaces for robot drivers
- **Debug Server** -- REST API (cpp-httplib) + Web UI (Chart.js) for real-time joint/sensor monitoring
- **Cross-Platform** -- Linux and Windows support via `PLUGIN_API` export macros

## Architecture

```
+------------------+     +-----------------+     +------------------+
|   Host App       |     |  PluginManager  |     |  Plugin (.so)    |
|                  |---->|                 |---->|                  |
|  main()          |     |  scan/load/     |     |  IPlugin         |
|  create plugins  |     |  start/stop     |     |  + Hardware      |
|  use services    |     |                 |     |  + MotionCtrl    |
+------------------+     +--------+--------+     |  + Vision        |
                                  |              +------------------+
                                  |
                    +-------------+-------------+
                    |             |             |
              +-----+-----+ +----+----+ +------+------+
              | EventBus  | | Service | | Dependency  |
              | pub/sub   | | Registry| | Resolver    |
              +-----------+ +---------+ +-------------+
```

### Core Modules

| Module | File | Description |
|--------|------|-------------|
| IPlugin | `plugin_interface.h` | Base interface with lifecycle methods and `RPF_PLUGIN_EXPORT` macro |
| PluginManager | `plugin_manager.h/cpp` | Orchestrates plugin loading, state transitions, and queries |
| PluginMetadata | `plugin_metadata.h` | Data model + JSON serialization for `.meta.json` companion files |
| EventBus | `event_bus.h` | Header-only publish/subscribe with per-handler exception isolation |
| ServiceRegistry | `service_registry.h` | Header-only type-safe service locator using `std::type_index` |
| DependencyResolver | `dependency_resolver.h/cpp` | Kahn's algorithm topological sort + DFS cycle detection |
| Hardware Interface | `hardware_interface.h` | Abstract `Hardware` and `Simulator` interfaces for robot drivers |
| Motion Controller | `motion_controller.h` | Abstract `MotionController` and `TrajectoryPlanner` interfaces |
| Vision Detector | `vision_detector.h` | Abstract `VisionDetector` and `PoseEstimator` interfaces |

## Directory Structure

```
robot_plugin_framework/
├── include/rpf/              # Public headers
├── src/                      # Core library sources
├── examples/
│   ├── host_app/             # CLI demo of full plugin lifecycle
│   ├── mujoco_hardware_plugin/   # 6-joint robot simulation (PD control)
│   ├── motion_control_plugin/    # Joint motion + trajectory planning
│   ├── vision_plugin/            # Object detection + pose estimation
│   └── debug_server/         # REST API server + Web UI
│       ├── debug_api.cpp     # 20+ REST endpoints
│       ├── main.cpp          # Server entry point
│       └── web/index.html    # Single-page Web console
├── tests/                    # GTest unit tests (205 cases)
├── third_party/              # FetchContent: dylib, nlohmann/json
└── CMakeLists.txt
```

## Requirements

- C++17 compiler (GCC 8+, Clang 7+, MSVC 2019+)
- CMake 3.14+
- Linux or Windows

Third-party dependencies are fetched automatically via CMake FetchContent:
- [dylib](https://github.com/martin-olivier/dylib) v2.2.1 -- cross-platform dynamic library loading
- [nlohmann/json](https://github.com/nlohmann/json) v3.11.3 -- JSON parsing
- [Google Test](https://github.com/google/googletest) v1.14.0 -- unit testing (optional)

## Build

```bash
cd robot_plugin_framework
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

Build options:

| Option | Default | Description |
|--------|---------|-------------|
| `RPF_BUILD_EXAMPLES` | ON | Build example plugins and host app |
| `RPF_BUILD_TESTS` | ON | Build unit tests |
| `RPF_SHARED` | ON | Build rpf as shared library (OFF for static) |

## Quick Start

### 1. Run the Debug Server

```bash
cd build/examples/debug_server
./debug_server --port 8080
```

The server auto-loads all plugins from `./plugins/` and starts the simulation.

### 2. Open the Web UI

Serve the web directory and open with the port parameter:

```bash
cd examples/debug_server/web
python3 -m http.server 8888
# Open: http://localhost:8888/?port=8080
```

The Web UI provides:
- **Plugin Management** -- scan, load, start plugins
- **Joint Control** -- 6 sliders with real-time position tracking (T: target, C: current)
- **Motion Planning** -- preset motions (Home, Ready, Wave, Point, Fold) and custom trajectory execution
- **Simulation Control** -- auto run, stop, reset, step (x1/x5/x10/x50)
- **Charts** -- real-time joint position and effort plots (Chart.js, 10Hz refresh)
- **Sensor Data** -- force, torque, and IMU readings

### 3. Run the CLI Demo

```bash
cd build/examples/host_app
./host_app
```

## Writing a Plugin

### Minimal Example

```cpp
#include "rpf/plugin_interface.h"

class MyPlugin : public rpf::IPlugin {
public:
    bool initialize() override {
        // Setup resources
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
        // Cleanup
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

### Metadata File

Create `my_plugin.meta.json` alongside the `.so`:

```json
{
    "name": "my_plugin",
    "version": "1.0.0",
    "description": "My custom plugin",
    "author": "Developer",
    "category": "custom",
    "provides": ["MyService"],
    "dependencies": [],
    "entry_point": "libmy_plugin.so"
}
```

### Providing Services

Override `getService()` to expose interfaces to other plugins:

```cpp
class MyPlugin : public rpf::IPlugin, public MyService {
public:
    void* getService(const std::string& name) override {
        if (name == "MyService") return static_cast<MyService*>(this);
        return nullptr;
    }
    // ... implement MyService methods
};
```

### Using the Hardware Interface

For robot hardware plugins, implement the `rpf::Hardware` interface:

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
    // ... other Hardware methods
};
```

## REST API Reference

All endpoints return JSON. Successful responses include `"success": true`. Error responses include `"error": "<message>"`.

### Plugin Management

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/plugins/scan` | List available plugins |
| GET | `/api/plugins/loaded` | List loaded plugins |
| GET | `/api/plugins/{name}/state` | Get plugin state |
| GET | `/api/plugins/{name}/metadata` | Get plugin metadata |
| POST | `/api/plugins/{name}/load` | Load plugin |
| POST | `/api/plugins/{name}/unload` | Unload plugin |
| POST | `/api/plugins/{name}/start` | Start plugin |
| POST | `/api/plugins/{name}/stop` | Stop plugin |

### Hardware Interface

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/robot/state` | Get all joint positions, velocities, efforts |
| GET | `/api/robot/sensors` | Get sensor data (force, torque, IMU) |
| GET | `/api/robot/joints/{name}/position?value=X` | Set single joint position |
| GET | `/api/robot/joints/positions?joint1=X&joint2=Y&...` | Set multiple joint positions |

### Simulation Control

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/simulation/start` | Start auto-stepping simulation |
| POST | `/api/simulation/stop` | Stop simulation |
| POST | `/api/simulation/reset` | Reset all joints to zero |
| POST | `/api/simulation/step` | Step simulation (body: `{"dt": 0.1}`) |

### Motion Planning

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/motion/plan?target=X,Y,Z,...&duration=T` | Plan and execute trajectory (6 values) |
| POST | `/api/motion/preset/{name}` | Execute preset: `home`, `ready`, `wave`, `point`, `fold` |
| POST | `/api/motion/stop` | Stop current motion |
| GET | `/api/motion/presets` | List available presets with targets |
| GET | `/api/motion/status` | Get motion execution status and progress |

### Services and Events

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/services` | List registered services |
| POST | `/api/events/publish` | Publish event (body: `{"event": "name", "data": {...}}`) |

## Motion Planning Details

The motion planner uses **smoothstep interpolation** between waypoints for natural-looking motion:

```
smoothstep(t) = t^2 * (3 - 2t)    where t in [0, 1]
```

Two-layer control architecture:

1. **Motion Thread** (20Hz) -- Interpolates trajectory waypoints and sets joint targets
2. **Simulation Thread** (100Hz) -- PD controller tracks targets with `Kp=2.0, Kd=2.0`

The PD controller computes:
```
effort = Kp * (target - position) - Kd * velocity
velocity += effort * dt
position += velocity * dt
```

## Example Plugins

| Plugin | Category | Provides | Description |
|--------|----------|----------|-------------|
| `mujoco_hardware` | hardware | Hardware, Simulator | 6-joint robot arm with PD control, force/torque/IMU sensors |
| `motion_control` | motion | MotionController, TrajectoryPlanner | Joint position control with linear interpolation |
| `vision` | vision | VisionDetector, PoseEstimator | Simulated object detection and pose estimation |

## CLI Options

```
./debug_server [options]
  --plugin-dir <dir>   Plugin directory (default: ./plugins)
  --port <port>        Server port (default: 8080)
  --help               Show help
```

## Testing

```bash
cd build
ctest --output-on-failure
```

Test suite: 205 unit tests across metadata, dependency resolver, event bus, service registry, and plugin manager.

## Thread Safety

- `PluginManager`, `EventBus`, `ServiceRegistry` use `std::shared_mutex` for concurrent read access
- Hardware plugin uses `std::mutex` for joint state protection
- Motion execution uses `std::atomic` flags for stop signaling

## License

See individual plugin metadata for licensing information.
