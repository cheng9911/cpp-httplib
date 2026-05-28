#include "debug_api.h"
#include "rpf/hardware_interface.h"
#include <iostream>
#include <sstream>

namespace robot {

DebugAPI::DebugAPI(rpf::PluginManager& plugin_manager, int port,
                   const std::string& hardware_plugin)
    : plugin_manager_(plugin_manager), hardware_plugin_name_(hardware_plugin),
      port_(port), running_(false) {
    server_ = std::make_unique<httplib::Server>();
}

rpf::Hardware* DebugAPI::getHardware() {
    auto* plugin = plugin_manager_.getPlugin(hardware_plugin_name_);
    if (!plugin) return nullptr;
    return static_cast<rpf::Hardware*>(plugin->getService("Hardware"));
}

rpf::Simulator* DebugAPI::getSimulator() {
    auto* plugin = plugin_manager_.getPlugin(hardware_plugin_name_);
    if (!plugin) return nullptr;
    return static_cast<rpf::Simulator*>(plugin->getService("Simulator"));
}

void DebugAPI::sendError(httplib::Response& res, int status, const std::string& message) {
    res.status = status;
    nlohmann::json error = {{"error", message}, {"success", false}};
    res.set_content(error.dump(2), "application/json");
}

void DebugAPI::sendSuccess(httplib::Response& res, const nlohmann::json& data) {
    nlohmann::json result = {{"success", true}};
    if (!data.is_null()) {
        result.update(data);
    }
    res.set_content(result.dump(2), "application/json");
}

DebugAPI::~DebugAPI() {
    // 停止正在执行的运动
    motion_stop_requested_ = true;
    if (motion_thread_.joinable()) {
        motion_thread_.join();
    }
    stop();
}

bool DebugAPI::start() {
    if (running_) {
        return true;
    }

    setupRoutes();

    // 在新线程中启动服务器
    server_thread_ = std::thread([this]() {
        std::cout << "[DebugAPI] Starting server on port " << port_ << std::endl;
        running_ = true;
        if (!server_->listen("0.0.0.0", port_)) {
            std::cerr << "[DebugAPI] Failed to start server" << std::endl;
            running_ = false;
        }
    });

    // 等待服务器启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return running_;
}

bool DebugAPI::stop() {
    if (!running_) {
        return true;
    }

    server_->stop();
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    running_ = false;
    std::cout << "[DebugAPI] Server stopped" << std::endl;
    return true;
}

bool DebugAPI::isRunning() const {
    return running_;
}

void DebugAPI::setupRoutes() {
    // CORS 预检请求处理
    server_->Options(R"(/api/.*)", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        res.status = 204;
    });

    // 添加CORS头到所有响应
    server_->set_post_routing_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        return httplib::Server::HandlerResponse::Unhandled;
    });

    // 插件管理API
    server_->Get("/api/plugins/scan", [this](const httplib::Request& req, httplib::Response& res) {
        handleScanPlugins(req, res);
    });

    server_->Get(R"(/api/plugins/([^/]+)/metadata)", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetPluginMetadata(req, res);
    });

    server_->Post(R"(/api/plugins/([^/]+)/load)", [this](const httplib::Request& req, httplib::Response& res) {
        handleLoadPlugin(req, res);
    });

    server_->Post(R"(/api/plugins/([^/]+)/unload)", [this](const httplib::Request& req, httplib::Response& res) {
        handleUnloadPlugin(req, res);
    });

    server_->Post(R"(/api/plugins/([^/]+)/start)", [this](const httplib::Request& req, httplib::Response& res) {
        handleStartPlugin(req, res);
    });

    server_->Post(R"(/api/plugins/([^/]+)/stop)", [this](const httplib::Request& req, httplib::Response& res) {
        handleStopPlugin(req, res);
    });

    server_->Get(R"(/api/plugins/([^/]+)/state)", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetPluginState(req, res);
    });

    server_->Get("/api/plugins/loaded", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetLoadedPlugins(req, res);
    });

    // 硬件接口API
    server_->Get("/api/robot/state", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetRobotState(req, res);
    });

    server_->Get(R"(/api/robot/joints/([^/]+)/position)", [this](const httplib::Request& req, httplib::Response& res) {
        handleSetJointPosition(req, res);
    });

    server_->Get("/api/robot/joints/positions", [this](const httplib::Request& req, httplib::Response& res) {
        handleSetJointPositions(req, res);
    });

    server_->Get("/api/robot/sensors", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetSensorData(req, res);
    });

    // 仿真控制API
    server_->Post("/api/simulation/start", [this](const httplib::Request& req, httplib::Response& res) {
        handleStartSimulation(req, res);
    });

    server_->Post("/api/simulation/stop", [this](const httplib::Request& req, httplib::Response& res) {
        handleStopSimulation(req, res);
    });

    server_->Post("/api/simulation/reset", [this](const httplib::Request& req, httplib::Response& res) {
        handleResetSimulation(req, res);
    });

    server_->Post("/api/simulation/step", [this](const httplib::Request& req, httplib::Response& res) {
        handleStepSimulation(req, res);
    });

    // 服务和事件API
    server_->Get("/api/services", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetServices(req, res);
    });

    server_->Post("/api/events/publish", [this](const httplib::Request& req, httplib::Response& res) {
        handlePublishEvent(req, res);
    });

    // 运动规划API (GET 避免 CORS preflight)
    server_->Get("/api/motion/plan", [this](const httplib::Request& req, httplib::Response& res) {
        handlePlanAndExecute(req, res);
    });

    server_->Post("/api/motion/stop", [this](const httplib::Request& req, httplib::Response& res) {
        handleStopMotion(req, res);
    });

    server_->Get("/api/motion/presets", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetPresets(req, res);
    });

    server_->Post(R"(/api/motion/preset/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        handleExecutePreset(req, res);
    });

    server_->Get("/api/motion/status", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetMotionStatus(req, res);
    });
}

void DebugAPI::handleScanPlugins(const httplib::Request& req, httplib::Response& res) {
    auto plugins = plugin_manager_.scanAvailablePlugins();
    nlohmann::json result = nlohmann::json::array();
    for (const auto& meta : plugins) {
        result.push_back({
            {"name", meta.name},
            {"version", meta.version},
            {"description", meta.description},
            {"author", meta.author},
            {"category", meta.category},
            {"provides", meta.provides}
        });
    }
    res.set_content(result.dump(2), "application/json");
}

void DebugAPI::handleGetPluginMetadata(const httplib::Request& req, httplib::Response& res) {
    std::string name = req.matches[1];
    try {
        auto meta = plugin_manager_.getPluginMetadata(name);
        nlohmann::json j = meta;
        res.set_content(j.dump(2), "application/json");
    } catch (const std::exception& e) {
        sendError(res, 404, "Plugin not found");
    }
}

void DebugAPI::handleLoadPlugin(const httplib::Request& req, httplib::Response& res) {
    std::string name = req.matches[1];
    bool success = plugin_manager_.loadPlugin(name);
    nlohmann::json result = {{"success", success}, {"plugin", name}};
    res.set_content(result.dump(2), "application/json");
}

void DebugAPI::handleUnloadPlugin(const httplib::Request& req, httplib::Response& res) {
    std::string name = req.matches[1];
    bool success = plugin_manager_.unloadPlugin(name);
    nlohmann::json result = {{"success", success}, {"plugin", name}};
    res.set_content(result.dump(2), "application/json");
}

void DebugAPI::handleStartPlugin(const httplib::Request& req, httplib::Response& res) {
    std::string name = req.matches[1];

    // 检查插件是否已经在运行
    auto state = plugin_manager_.getPluginState(name);
    if (state == rpf::PluginState::Running) {
        nlohmann::json result = {{"success", true}, {"plugin", name}, {"message", "Already running"}};
        res.set_content(result.dump(2), "application/json");
        return;
    }

    bool success = false;
    if (state == rpf::PluginState::Loaded) {
        success = plugin_manager_.initializePlugin(name) && plugin_manager_.startPlugin(name);
    } else if (state == rpf::PluginState::Initialized) {
        success = plugin_manager_.startPlugin(name);
    } else {
        // 需要先加载
        success = plugin_manager_.loadPlugin(name) &&
                  plugin_manager_.initializePlugin(name) &&
                  plugin_manager_.startPlugin(name);
    }

    nlohmann::json result = {{"success", success}, {"plugin", name}};
    res.set_content(result.dump(2), "application/json");
}

void DebugAPI::handleStopPlugin(const httplib::Request& req, httplib::Response& res) {
    std::string name = req.matches[1];
    bool success = plugin_manager_.stopPlugin(name);
    nlohmann::json result = {{"success", success}, {"plugin", name}};
    res.set_content(result.dump(2), "application/json");
}

void DebugAPI::handleGetPluginState(const httplib::Request& req, httplib::Response& res) {
    std::string name = req.matches[1];
    auto state = plugin_manager_.getPluginState(name);
    nlohmann::json result = {
        {"plugin", name},
        {"state", rpf::pluginStateToString(state)}
    };
    res.set_content(result.dump(2), "application/json");
}

void DebugAPI::handleGetLoadedPlugins(const httplib::Request& req, httplib::Response& res) {
    auto plugins = plugin_manager_.getLoadedPlugins();
    nlohmann::json result = plugins;
    res.set_content(result.dump(2), "application/json");
}

void DebugAPI::handleGetRobotState(const httplib::Request& req, httplib::Response& res) {
    auto* hardware = getHardware();
    if (!hardware) {
        sendError(res, 404, "Hardware plugin not loaded");
        return;
    }

    auto state = hardware->getState();
    nlohmann::json result = {
        {"timestamp", state.timestamp},
        {"is_moving", state.is_moving},
        {"joints", nlohmann::json::array()}
    };

    for (const auto& joint : state.joints) {
        result["joints"].push_back({
            {"name", joint.name},
            {"position", joint.position},
            {"velocity", joint.velocity},
            {"effort", joint.effort}
        });
    }

    res.set_content(result.dump(2), "application/json");
}

void DebugAPI::handleSetJointPosition(const httplib::Request& req, httplib::Response& res) {
    std::string joint_name = req.matches[1];

    auto* hardware = getHardware();
    if (!hardware) {
        sendError(res, 404, "Hardware plugin not loaded");
        return;
    }

    // 从查询参数读取 position: /api/robot/joints/joint1/position?value=-0.5
    if (!req.has_param("value")) {
        sendError(res, 400, "Missing 'value' query parameter");
        return;
    }
    double position = 0.0;
    try {
        position = std::stod(req.get_param_value("value"));
    } catch (...) {
        sendError(res, 400, "Invalid position value");
        return;
    }
    if (std::isnan(position) || std::isinf(position)) {
        sendError(res, 400, "Invalid position value");
        return;
    }
    bool success = hardware->setJointPosition(joint_name, position);
    sendSuccess(res, {{"joint", joint_name}, {"position", position}});
}

void DebugAPI::handleSetJointPositions(const httplib::Request& req, httplib::Response& res) {
    auto* hardware = getHardware();
    if (!hardware) {
        sendError(res, 404, "Hardware plugin not loaded");
        return;
    }

    // 从查询参数读取: /api/robot/joints/positions?joint1=0&joint2=-0.5&joint3=1.0
    auto params = req.params;
    if (params.empty()) {
        sendError(res, 400, "Missing query parameters (e.g. ?joint1=0&joint2=-0.5)");
        return;
    }

    std::map<std::string, double> positions;
    for (const auto& [key, value] : params) {
        try {
            double pos = std::stod(value);
            if (std::isnan(pos) || std::isinf(pos)) {
                sendError(res, 400, "Invalid value for " + key);
                return;
            }
            positions[key] = pos;
        } catch (...) {
            sendError(res, 400, "Invalid numeric value for " + key);
            return;
        }
    }

    bool success = hardware->setJointPositions(positions);
    sendSuccess(res);
}

void DebugAPI::handleGetSensorData(const httplib::Request& req, httplib::Response& res) {
    auto* hardware = getHardware();
    if (!hardware) {
        sendError(res, 404, "Hardware plugin not loaded");
        return;
    }

    auto sensors = hardware->getSensorData();
    nlohmann::json result = nlohmann::json::array();
    for (const auto& sensor : sensors) {
        result.push_back({
            {"name", sensor.name},
            {"type", sensor.type},
            {"values", sensor.values}
        });
    }

    res.set_content(result.dump(2), "application/json");
}

void DebugAPI::handleStartSimulation(const httplib::Request& req, httplib::Response& res) {
    auto* hardware = getHardware();
    if (!hardware) {
        sendError(res, 404, "Hardware plugin not loaded");
        return;
    }

    bool success = hardware->startSimulation();
    sendSuccess(res);
}

void DebugAPI::handleStopSimulation(const httplib::Request& req, httplib::Response& res) {
    auto* hardware = getHardware();
    if (!hardware) {
        sendError(res, 404, "Hardware plugin not loaded");
        return;
    }

    bool success = hardware->stopSimulation();
    sendSuccess(res);
}

void DebugAPI::handleResetSimulation(const httplib::Request& req, httplib::Response& res) {
    auto* hardware = getHardware();
    if (!hardware) {
        sendError(res, 404, "Hardware plugin not loaded");
        return;
    }

    bool success = hardware->reset();
    sendSuccess(res);
}

void DebugAPI::handleStepSimulation(const httplib::Request& req, httplib::Response& res) {
    auto* hardware = getHardware();
    if (!hardware) {
        sendError(res, 404, "Hardware plugin not loaded");
        return;
    }

    double dt = 0.001;
    if (!req.body.empty()) {
        try {
            auto body = nlohmann::json::parse(req.body);
            if (body.contains("dt")) {
                dt = body["dt"].get<double>();
                if (dt <= 0.0 || dt > 10.0) {
                    sendError(res, 400, "dt must be in (0, 10]");
                    return;
                }
            }
        } catch (...) {}
    }
    bool success = hardware->stepSimulation(dt);
    sendSuccess(res, {{"dt", dt}});
}

void DebugAPI::handleGetServices(const httplib::Request& req, httplib::Response& res) {
    auto& registry = plugin_manager_.getServiceRegistry();
    auto services = registry.listServices();
    nlohmann::json result = services;
    res.set_content(result.dump(2), "application/json");
}

void DebugAPI::handlePublishEvent(const httplib::Request& req, httplib::Response& res) {
    try {
        auto body = nlohmann::json::parse(req.body);
        if (!body.contains("event") || !body["event"].is_string()) {
            sendError(res, 400, "Missing or invalid 'event' field");
            return;
        }
        std::string event = body["event"].get<std::string>();
        if (event.empty()) {
            sendError(res, 400, "Event name cannot be empty");
            return;
        }
        nlohmann::json data = body.value("data", nlohmann::json::object());

        auto& event_bus = plugin_manager_.getEventBus();
        event_bus.publish(event, data);

        sendSuccess(res, {{"event", event}});
    } catch (const std::exception& e) {
        sendError(res, 400, "Invalid request body");
    }
}

// ===== 运动规划 API 实现 =====

void DebugAPI::executeTrajectory(const std::vector<std::vector<double>>& waypoints, double duration) {
    try {
    auto* hardware = getHardware();
    if (!hardware || waypoints.empty()) {
        motion_executing_ = false;
        return;
    }

    motion_executing_ = true;
    motion_stop_requested_ = false;
    motion_progress_ = 0.0;

    // 停止自动仿真，使用手动步进控制
    hardware->stopSimulation();

    auto joint_names = hardware->getState().joints;
    size_t num_joints = joint_names.size();

    double sim_dt = 0.05;
    int steps_per_second = 20;
    int total_steps = static_cast<int>(duration * steps_per_second);
    if (total_steps < 1) total_steps = 1;

    for (int step = 0; step <= total_steps; ++step) {
        if (motion_stop_requested_) break;

        double t = static_cast<double>(step) / total_steps;
        motion_progress_ = t * 100.0;

        size_t num_segments = waypoints.size() - 1;
        double segment_t = t * num_segments;
        size_t wp_idx = static_cast<size_t>(segment_t);
        wp_idx = std::min(wp_idx, num_segments - 1);
        double local_t = segment_t - wp_idx;
        local_t = std::clamp(local_t, 0.0, 1.0);

        double smooth_t = local_t * local_t * (3 - 2 * local_t);

        std::map<std::string, double> positions;
        for (size_t i = 0; i < num_joints && i < waypoints[wp_idx].size(); ++i) {
            double start = waypoints[wp_idx][i];
            double end = (wp_idx + 1 < waypoints.size()) ? waypoints[wp_idx + 1][i] : start;
            double pos = start + (end - start) * smooth_t;
            positions[joint_names[i].name] = pos;
        }

        hardware->setJointPositions(positions);
        hardware->stepSimulation(sim_dt);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // 额外等待让 PD 控制器收敛到目标
    for (int i = 0; i < 60; ++i) {
        if (motion_stop_requested_) break;
        hardware->stepSimulation(sim_dt);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    motion_progress_ = 100.0;
    motion_executing_ = false;

    // 重启自动仿真线程
    hardware->startSimulation();
    } catch (const std::exception& e) {
        std::cerr << "[Motion] Exception: " << e.what() << std::endl;
        motion_executing_ = false;
    } catch (...) {
        std::cerr << "[Motion] Unknown exception" << std::endl;
        motion_executing_ = false;
    }
}

void DebugAPI::handlePlanAndExecute(const httplib::Request& req, httplib::Response& res) {
    auto* hardware = getHardware();
    if (!hardware) {
        sendError(res, 404, "Hardware plugin not loaded");
        return;
    }

    if (motion_executing_) {
        sendError(res, 409, "Motion already in progress");
        return;
    }

    // 从查询参数读取: /api/motion/plan?target=0,-0.5,1.0,0,0.5,0&duration=2.0
    if (!req.has_param("target")) {
        sendError(res, 400, "Missing 'target' query parameter (comma-separated values)");
        return;
    }

    std::string target_str = req.get_param_value("target");
    std::vector<double> target;
    std::istringstream ss(target_str);
    std::string token;
    while (std::getline(ss, token, ',')) {
        try {
            double val = std::stod(token);
            if (std::isnan(val) || std::isinf(val)) {
                sendError(res, 400, "Invalid target value: " + token);
                return;
            }
            target.push_back(val);
        } catch (...) {
            sendError(res, 400, "Invalid numeric value in target: " + token);
            return;
        }
    }

    if (target.size() != 6) {
        sendError(res, 400, "Target must have exactly 6 values (got " + std::to_string(target.size()) + ")");
        return;
    }

    double duration = 2.0;
    if (req.has_param("duration")) {
        try {
            duration = std::stod(req.get_param_value("duration"));
        } catch (...) {
            sendError(res, 400, "Invalid duration value");
            return;
        }
    }
    if (duration <= 0.0 || duration > 30.0) {
        sendError(res, 400, "Duration must be in (0, 30]");
        return;
    }

    // 获取当前位置作为起点
    auto state = hardware->getState();
    std::vector<double> start;
    for (const auto& joint : state.joints) {
        start.push_back(joint.position);
    }

    // 构建轨迹点（起点 → 终点）
    std::vector<std::vector<double>> waypoints = {start, target};

    // 在后台线程执行轨迹
    if (motion_thread_.joinable()) {
        motion_thread_.join();
    }
    motion_thread_ = std::thread(&DebugAPI::executeTrajectory, this, waypoints, duration);

    sendSuccess(res, {{"duration", duration}, {"waypoints", 2}});
}

void DebugAPI::handleStopMotion(const httplib::Request& req, httplib::Response& res) {
    if (!motion_executing_) {
        sendSuccess(res, {{"message", "No motion in progress"}});
        return;
    }

    motion_stop_requested_ = true;
    if (motion_thread_.joinable()) {
        motion_thread_.join();
    }
    sendSuccess(res, {{"message", "Motion stopped"}});
}

void DebugAPI::handleGetPresets(const httplib::Request& req, httplib::Response& res) {
    nlohmann::json presets = {
        {"home", {
            {"name", "Home"},
            {"description", "所有关节归零"},
            {"target", {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
            {"duration", 2.0}
        }},
        {"ready", {
            {"name", "Ready"},
            {"description", "准备姿态"},
            {"target", {0.0, -0.5, 1.0, 0.0, 0.5, 0.0}},
            {"duration", 2.0}
        }},
        {"wave", {
            {"name", "Wave"},
            {"description", "挥手动作"},
            {"target", {0.0, -1.0, 1.5, 0.0, 1.0, 0.0}},
            {"duration", 1.5}
        }},
        {"point", {
            {"name", "Point"},
            {"description", "指向动作"},
            {"target", {1.57, -0.3, 0.8, 0.0, 0.5, 0.0}},
            {"duration", 2.0}
        }},
        {"fold", {
            {"name", "Fold"},
            {"description", "折叠收纳"},
            {"target", {0.0, -2.5, 2.5, 0.0, -1.5, 0.0}},
            {"duration", 3.0}
        }}
    };
    res.set_content(presets.dump(2), "application/json");
}

void DebugAPI::handleExecutePreset(const httplib::Request& req, httplib::Response& res) {
    auto* hardware = getHardware();
    if (!hardware) {
        sendError(res, 404, "Hardware plugin not loaded");
        return;
    }

    if (motion_executing_) {
        sendError(res, 409, "Motion already in progress");
        return;
    }

    std::string preset_name = req.matches[1];

    // 预设动作定义
    std::map<std::string, std::pair<std::vector<double>, double>> presets = {
        {"home",  {{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}, 2.0}},
        {"ready", {{0.0, -0.5, 1.0, 0.0, 0.5, 0.0}, 2.0}},
        {"wave",  {{0.0, -1.0, 1.5, 0.0, 1.0, 0.0}, 1.5}},
        {"point", {{1.57, -0.3, 0.8, 0.0, 0.5, 0.0}, 2.0}},
        {"fold",  {{0.0, -2.5, 2.5, 0.0, -1.5, 0.0}, 3.0}}
    };

    auto it = presets.find(preset_name);
    if (it == presets.end()) {
        sendError(res, 404, "Unknown preset: " + preset_name);
        return;
    }

    // 获取当前位置
    auto state = hardware->getState();
    std::vector<double> start;
    for (const auto& joint : state.joints) {
        start.push_back(joint.position);
    }

    // 构建轨迹
    std::vector<std::vector<double>> waypoints = {start, it->second.first};

    // 对 wave 动作添加中间点使其更生动
    if (preset_name == "wave") {
        std::vector<double> mid1 = {0.0, -0.8, 1.2, 0.0, 0.8, 0.0};
        std::vector<double> mid2 = {0.0, -1.2, 1.8, 0.0, 1.2, 0.0};
        std::vector<double> mid3 = {0.0, -0.8, 1.2, 0.0, 0.8, 0.0};
        waypoints = {start, mid1, mid2, mid3, it->second.first};
    }

    // 在后台线程执行
    if (motion_thread_.joinable()) {
        motion_thread_.join();
    }
    double duration = it->second.second;
    if (preset_name == "wave") duration = 3.0;
    motion_thread_ = std::thread(&DebugAPI::executeTrajectory, this, waypoints, duration);

    sendSuccess(res, {{"preset", preset_name}, {"duration", duration}});
}

void DebugAPI::handleGetMotionStatus(const httplib::Request& req, httplib::Response& res) {
    nlohmann::json status = {
        {"executing", motion_executing_.load()},
        {"progress", motion_progress_.load()}
    };
    res.set_content(status.dump(2), "application/json");
}

} // namespace robot
