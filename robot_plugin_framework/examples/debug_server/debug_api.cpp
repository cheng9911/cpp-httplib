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

    server_->Post(R"(/api/robot/joints/([^/]+)/position)", [this](const httplib::Request& req, httplib::Response& res) {
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

    try {
        auto body = nlohmann::json::parse(req.body);
        if (!body.contains("position") || !body["position"].is_number()) {
            sendError(res, 400, "Missing or invalid 'position' field");
            return;
        }
        double position = body["position"].get<double>();
        if (std::isnan(position) || std::isinf(position)) {
            sendError(res, 400, "Invalid position value");
            return;
        }
        bool success = hardware->setJointPosition(joint_name, position);
        sendSuccess(res, {{"joint", joint_name}, {"position", position}});
    } catch (const std::exception& e) {
        sendError(res, 400, "Invalid request body");
    }
}

void DebugAPI::handleSetJointPositions(const httplib::Request& req, httplib::Response& res) {
    auto* hardware = getHardware();
    if (!hardware) {
        sendError(res, 404, "Hardware plugin not loaded");
        return;
    }

    try {
        auto body = nlohmann::json::parse(req.body);
        if (!body.is_object()) {
            sendError(res, 400, "Request body must be a JSON object");
            return;
        }
        std::map<std::string, double> positions;
        for (auto& [key, value] : body.items()) {
            if (!value.is_number()) {
                sendError(res, 400, "Position values must be numbers");
                return;
            }
            positions[key] = value.get<double>();
        }
        bool success = hardware->setJointPositions(positions);
        sendSuccess(res);
    } catch (const std::exception& e) {
        sendError(res, 400, "Invalid request body");
    }
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

} // namespace robot
