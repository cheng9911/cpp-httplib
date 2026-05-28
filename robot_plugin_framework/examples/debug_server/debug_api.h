#pragma once

#include "rpf/plugin_manager.h"
#include "httplib.h"
#include <memory>
#include <string>

namespace robot {

class DebugAPI {
public:
    DebugAPI(rpf::PluginManager& plugin_manager, int port = 8080);
    ~DebugAPI();

    // 启动/停止服务器
    bool start();
    bool stop();
    bool isRunning() const;

private:
    void setupRoutes();

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

    rpf::PluginManager& plugin_manager_;
    std::unique_ptr<httplib::Server> server_;
    int port_;
    bool running_;
    std::thread server_thread_;
};

} // namespace robot
