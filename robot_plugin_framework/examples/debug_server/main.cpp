#include "debug_api.h"
#include <iostream>
#include <filesystem>
#include <signal.h>

namespace fs = std::filesystem;

std::unique_ptr<robot::DebugAPI> debug_api;

void signalHandler(int signum) {
    std::cout << "\nReceived signal " << signum << ", shutting down..." << std::endl;
    if (debug_api) {
        debug_api->stop();
    }
    exit(signum);
}

int main(int argc, char* argv[]) {
    // 注册信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // 解析命令行参数
    std::string plugin_dir = "./plugins";
    int port = 8080;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--plugin-dir" && i + 1 < argc) {
            plugin_dir = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --plugin-dir <dir>  Plugin directory (default: ./plugins)" << std::endl;
            std::cout << "  --port <port>       Server port (default: 8080)" << std::endl;
            std::cout << "  --help              Show this help" << std::endl;
            return 0;
        }
    }

    std::cout << "=== Robot Debug Server ===" << std::endl;
    std::cout << "Plugin directory: " << plugin_dir << std::endl;
    std::cout << "Server port: " << port << std::endl;

    // 创建插件管理器
    rpf::PluginManager plugin_manager(plugin_dir);

    // 扫描并加载插件
    std::cout << "\nScanning plugins..." << std::endl;
    auto plugins = plugin_manager.scanAvailablePlugins();
    std::cout << "Found " << plugins.size() << " plugins" << std::endl;

    for (const auto& meta : plugins) {
        std::cout << "Loading " << meta.name << "..." << std::endl;
        if (plugin_manager.loadPlugin(meta.name)) {
            plugin_manager.initializePlugin(meta.name);
            plugin_manager.startPlugin(meta.name);
            std::cout << "  -> Started" << std::endl;
        } else {
            std::cerr << "  -> Failed to load" << std::endl;
        }
    }

    // 创建并启动调试API服务器
    debug_api = std::make_unique<robot::DebugAPI>(plugin_manager, port);

    std::cout << "\nStarting debug server..." << std::endl;
    std::cout << "API endpoints:" << std::endl;
    std::cout << "  GET  /api/plugins/scan           - Scan available plugins" << std::endl;
    std::cout << "  GET  /api/plugins/loaded          - Get loaded plugins" << std::endl;
    std::cout << "  GET  /api/robot/state             - Get robot state" << std::endl;
    std::cout << "  GET  /api/robot/sensors           - Get sensor data" << std::endl;
    std::cout << "  POST /api/robot/joints/{name}/position - Set joint position" << std::endl;
    std::cout << "  POST /api/simulation/start        - Start simulation" << std::endl;
    std::cout << "  POST /api/simulation/stop         - Stop simulation" << std::endl;
    std::cout << "  POST /api/simulation/reset        - Reset simulation" << std::endl;
    std::cout << "\nPress Ctrl+C to stop" << std::endl;

    if (!debug_api->start()) {
        std::cerr << "Failed to start debug server" << std::endl;
        return 1;
    }

    // 保持主线程运行
    while (debug_api->isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}
