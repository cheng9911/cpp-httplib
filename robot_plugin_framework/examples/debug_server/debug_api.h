#pragma once

#include "rpf/plugin_manager.h"
#include "rpf/hardware_interface.h"
#include "httplib.h"
#include <memory>
#include <string>

namespace robot {

class DebugAPI {
public:
    DebugAPI(rpf::PluginManager& plugin_manager, int port = 8080,
             const std::string& hardware_plugin = "mujoco_hardware");
    ~DebugAPI();

    // 启动/停止服务器
    bool start();
    bool stop();
    bool isRunning() const;

private:
    void setupRoutes();

    // 辅助函数
    rpf::Hardware* getHardware();
    rpf::Simulator* getSimulator();
    void sendError(httplib::Response& res, int status, const std::string& message);
    void sendSuccess(httplib::Response& res, const nlohmann::json& data = {});

    // 插件管理API
    void handleScanPlugins(const httplib::Request& req, httplib::Response& res);
    void handleGetPluginMetadata(const httplib::Request& req, httplib::Response& res);
    void handleLoadPlugin(const httplib::Request& req, httplib::Response& res);
    void handleUnloadPlugin(const httplib::Request& req, httplib::Response& res);
    void handleStartPlugin(const httplib::Request& req, httplib::Response& res);
    void handleStopPlugin(const httplib::Request& req, httplib::Response& res);
    void handleGetPluginState(const httplib::Request& req, httplib::Response& res);
    void handleGetLoadedPlugins(const httplib::Request& req, httplib::Response& res);

    // 硬件接口API
    void handleGetRobotState(const httplib::Request& req, httplib::Response& res);
    void handleSetJointPosition(const httplib::Request& req, httplib::Response& res);
    void handleSetJointPositions(const httplib::Request& req, httplib::Response& res);
    void handleGetSensorData(const httplib::Request& req, httplib::Response& res);

    // 仿真控制API
    void handleStartSimulation(const httplib::Request& req, httplib::Response& res);
    void handleStopSimulation(const httplib::Request& req, httplib::Response& res);
    void handleResetSimulation(const httplib::Request& req, httplib::Response& res);
    void handleStepSimulation(const httplib::Request& req, httplib::Response& res);

    // 服务和事件API
    void handleGetServices(const httplib::Request& req, httplib::Response& res);
    void handlePublishEvent(const httplib::Request& req, httplib::Response& res);

    // 运动规划API
    void handlePlanAndExecute(const httplib::Request& req, httplib::Response& res);
    void handleStopMotion(const httplib::Request& req, httplib::Response& res);
    void handleGetPresets(const httplib::Request& req, httplib::Response& res);
    void handleExecutePreset(const httplib::Request& req, httplib::Response& res);
    void handleGetMotionStatus(const httplib::Request& req, httplib::Response& res);

    // 运动执行
    void executeTrajectory(const std::vector<std::vector<double>>& waypoints, double duration);

    rpf::PluginManager& plugin_manager_;
    std::unique_ptr<httplib::Server> server_;
    std::string hardware_plugin_name_;
    int port_;
    bool running_;
    std::thread server_thread_;

    // 运动执行状态
    std::atomic<bool> motion_executing_{false};
    std::atomic<bool> motion_stop_requested_{false};
    std::atomic<double> motion_progress_{0.0};
    std::thread motion_thread_;
    std::mutex motion_mutex_;
};

} // namespace robot
