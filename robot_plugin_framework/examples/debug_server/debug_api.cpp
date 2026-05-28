#include "debug_api.h"
#include "rpf/hardware_interface.h"
#include <iostream>
#include <sstream>

namespace robot {

DebugAPI::DebugAPI(rpf::PluginManager& plugin_manager, int port)
    : plugin_manager_(plugin_manager), port_(port), running_(false) {
    server_ = std::make_unique<httplib::Server>();
}

DebugAPI::~DebugAPI() {
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

    server_->Get(R"(/api/plugins/(\w+)/metadata)", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetPluginMetadata(req, res);
    });

    server_->Post(R"(/api/plugins/(\w+)/load)", [this](const httplib::Request& req, httplib::Response& res) {
        handleLoadPlugin(req, res);
    });

    server_->Post(R"(/api/plugins/(\w+)/unload)", [this](const httplib::Request& req, httplib::Response& res) {
        handleUnloadPlugin(req, res);
    });

    server_->Post(R"(/api/plugins/(\w+)/start)", [this](const httplib::Request& req, httplib::Response& res) {
        handleStartPlugin(req, res);
    });

    server_->Post(R"(/api/plugins/(\w+)/stop)", [this](const httplib::Request& req, httplib::Response& res) {
        handleStopPlugin(req, res);
    });

    server_->Get(R"(/api/plugins/(\w+)/state)", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetPluginState(req, res);
    });

    server_->Get("/api/plugins/loaded", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetLoadedPlugins(req, res);
    });

    // 硬件接口API
    server_->Get("/api/robot/state", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetRobotState(req, res);
    });

    server_->Post(R"(/api/robot/joints/(\w+)/position)", [this](const httplib::Request& req, httplib::Response& res) {
        handleSetJointPosition(req, res);
    });

    server_->Post("/api/robot/joints/positions", [this](const httplib::Request& req, httplib::Response& res) {
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
        res.status = 404;
        res.set_content(R"({"error": "Plugin not found"})", "application/json");
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
    auto* hardware_plugin = plugin_manager_.getPlugin("mujoco_hardware");
    if (!hardware_plugin) {
        res.status = 404;
        res.set_content(R"({"error": "Hardware plugin not loaded"})", "application/json");
        return;
    }

    auto* hardware = static_cast<rpf::Hardware*>(hardware_plugin->getService("Hardware"));
    if (!hardware) {
        res.status = 500;
        res.set_content(R"({"error": "Hardware service not available"})", "application/json");
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

    auto* hardware_plugin = plugin_manager_.getPlugin("mujoco_hardware");
    if (!hardware_plugin) {
        res.status = 404;
        res.set_content(R"({"error": "Hardware plugin not loaded"})", "application/json");
        return;
    }

    auto* hardware = static_cast<rpf::Hardware*>(hardware_plugin->getService("Hardware"));
    if (!hardware) {
        res.status = 500;
        res.set_content(R"({"error": "Hardware service not available"})", "application/json");
        return;
    }

    try {
        auto body = nlohmann::json::parse(req.body);
        double position = body["position"];
        bool success = hardware->setJointPosition(joint_name, position);
        nlohmann::json result = {{"success", success}, {"joint", joint_name}, {"position", position}};
        res.set_content(result.dump(2), "application/json");
    } catch (const std::exception& e) {
        res.status = 400;
        res.set_content(R"({"error": "Invalid request body"})", "application/json");
    }
}

void DebugAPI::handleSetJointPositions(const httplib::Request& req, httplib::Response& res) {
    auto* hardware_plugin = plugin_manager_.getPlugin("mujoco_hardware");
    if (!hardware_plugin) {
        res.status = 404;
        res.set_content(R"({"error": "Hardware plugin not loaded"})", "application/json");
        return;
    }

    auto* hardware = static_cast<rpf::Hardware*>(hardware_plugin->getService("Hardware"));
    if (!hardware) {
        res.status = 500;
        res.set_content(R"({"error": "Hardware service not available"})", "application/json");
        return;
    }

    try {
        auto body = nlohmann::json::parse(req.body);
        std::map<std::string, double> positions;
        for (auto& [key, value] : body.items()) {
            positions[key] = value.get<double>();
        }
        bool success = hardware->setJointPositions(positions);
        nlohmann::json result = {{"success", success}};
        res.set_content(result.dump(2), "application/json");
    } catch (const std::exception& e) {
        res.status = 400;
        res.set_content(R"({"error": "Invalid request body"})", "application/json");
    }
}

void DebugAPI::handleGetSensorData(const httplib::Request& req, httplib::Response& res) {
    auto* hardware_plugin = plugin_manager_.getPlugin("mujoco_hardware");
    if (!hardware_plugin) {
        res.status = 404;
        res.set_content(R"({"error": "Hardware plugin not loaded"})", "application/json");
        return;
    }

    auto* hardware = static_cast<rpf::Hardware*>(hardware_plugin->getService("Hardware"));
    if (!hardware) {
        res.status = 500;
        res.set_content(R"({"error": "Hardware service not available"})", "application/json");
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
    auto* hardware_plugin = plugin_manager_.getPlugin("mujoco_hardware");
    if (!hardware_plugin) {
        res.status = 404;
        res.set_content(R"({"error": "Hardware plugin not loaded"})", "application/json");
        return;
    }

    auto* simulator = static_cast<rpf::Simulator*>(hardware_plugin->getService("Simulator"));
    if (!simulator) {
        res.status = 500;
        res.set_content(R"({"error": "Simulator service not available"})", "application/json");
        return;
    }

    // 获取Hardware接口来启动仿真
    auto* hardware = static_cast<rpf::Hardware*>(hardware_plugin->getService("Hardware"));
    bool success = hardware->startSimulation();
    nlohmann::json result = {{"success", success}};
    res.set_content(result.dump(2), "application/json");
}

void DebugAPI::handleStopSimulation(const httplib::Request& req, httplib::Response& res) {
    auto* hardware_plugin = plugin_manager_.getPlugin("mujoco_hardware");
    if (!hardware_plugin) {
        res.status = 404;
        res.set_content(R"({"error": "Hardware plugin not loaded"})", "application/json");
        return;
    }

    auto* hardware = static_cast<rpf::Hardware*>(hardware_plugin->getService("Hardware"));
    bool success = hardware->stopSimulation();
    nlohmann::json result = {{"success", success}};
    res.set_content(result.dump(2), "application/json");
}

void DebugAPI::handleResetSimulation(const httplib::Request& req, httplib::Response& res) {
    auto* hardware_plugin = plugin_manager_.getPlugin("mujoco_hardware");
    if (!hardware_plugin) {
        res.status = 404;
        res.set_content(R"({"error": "Hardware plugin not loaded"})", "application/json");
        return;
    }

    auto* hardware = static_cast<rpf::Hardware*>(hardware_plugin->getService("Hardware"));
    bool success = hardware->reset();
    nlohmann::json result = {{"success", success}};
    res.set_content(result.dump(2), "application/json");
}

void DebugAPI::handleStepSimulation(const httplib::Request& req, httplib::Response& res) {
    auto* hardware_plugin = plugin_manager_.getPlugin("mujoco_hardware");
    if (!hardware_plugin) {
        res.status = 404;
        res.set_content(R"({"error": "Hardware plugin not loaded"})", "application/json");
        return;
    }

    auto* hardware = static_cast<rpf::Hardware*>(hardware_plugin->getService("Hardware"));
    double dt = 0.001;
    if (!req.body.empty()) {
        try {
            auto body = nlohmann::json::parse(req.body);
            if (body.contains("dt")) {
                dt = body["dt"].get<double>();
            }
        } catch (...) {}
    }
    bool success = hardware->stepSimulation(dt);
    nlohmann::json result = {{"success", success}, {"dt", dt}};
    res.set_content(result.dump(2), "application/json");
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
        std::string event = body["event"];
        nlohmann::json data = body.value("data", nlohmann::json{});

        auto& event_bus = plugin_manager_.getEventBus();
        event_bus.publish(event, data);

        nlohmann::json result = {{"success", true}, {"event", event}};
        res.set_content(result.dump(2), "application/json");
    } catch (const std::exception& e) {
        res.status = 400;
        res.set_content(R"({"error": "Invalid request body"})", "application/json");
    }
}

} // namespace robot
