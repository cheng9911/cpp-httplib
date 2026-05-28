#include "rpf/plugin_manager.h"
#include "rpf/motion_controller.h"
#include "rpf/vision_detector.h"
#include <iostream>
#include <iomanip>

void printSeparator(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================\n" << std::endl;
}

int main() {
    std::cout << "Robot Plugin Framework - Host Application" << std::endl;

    // 获取插件目录 (从命令行参数或使用默认路径)
    std::string plugin_dir = "./plugins";
    if (!std::filesystem::exists(plugin_dir)) {
        plugin_dir = ".";  // 尝试当前目录
    }

    rpf::PluginManager manager(plugin_dir);

    // Step 1: 扫描可用插件 (不加载)
    printSeparator("Step 1: Scan Available Plugins");
    auto available_plugins = manager.scanAvailablePlugins();
    std::cout << "Found " << available_plugins.size() << " plugins:" << std::endl;
    for (const auto& meta : available_plugins) {
        std::cout << "  - " << meta.name << " v" << meta.version << std::endl;
        std::cout << "    Description: " << meta.description << std::endl;
        std::cout << "    Author: " << meta.author << std::endl;
        std::cout << "    Category: " << meta.category << std::endl;
        std::cout << "    Provides: ";
        for (const auto& service : meta.provides) {
            std::cout << service << " ";
        }
        std::cout << std::endl;
        if (!meta.dependencies.empty()) {
            std::cout << "    Dependencies: ";
            for (const auto& dep : meta.dependencies) {
                std::cout << dep.name << " ";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }

    // Step 2: 加载插件
    printSeparator("Step 2: Load Plugins");
    for (const auto& meta : available_plugins) {
        std::cout << "Loading " << meta.name << "..." << std::endl;
        if (manager.loadPlugin(meta.name)) {
            std::cout << "  -> Loaded successfully" << std::endl;
        } else {
            std::cerr << "  -> Failed to load" << std::endl;
        }
    }

    // Step 3: 初始化并启动插件
    printSeparator("Step 3: Initialize and Start Plugins");
    for (const auto& meta : available_plugins) {
        std::cout << "Initializing " << meta.name << "..." << std::endl;
        if (manager.initializePlugin(meta.name)) {
            std::cout << "  -> Initialized" << std::endl;
            std::cout << "Starting " << meta.name << "..." << std::endl;
            if (manager.startPlugin(meta.name)) {
                std::cout << "  -> Started" << std::endl;
            }
        }
    }

    // Step 4: 使用插件服务
    printSeparator("Step 4: Use Plugin Services");

    // 获取运动控制器服务
    auto* motion_plugin = manager.getPlugin("motion_control");
    if (motion_plugin) {
        auto* controller = static_cast<robot::MotionController*>(
            motion_plugin->getService("MotionController"));
        if (controller) {
            std::cout << "Using MotionController:" << std::endl;
            std::cout << "  Joint count: " << controller->getJointCount() << std::endl;
            controller->setJointPosition(0, 1.57);
            controller->setJointPosition(1, -0.5);
            std::cout << "  Joint 0 position: " << controller->getJointPosition(0) << std::endl;
            std::cout << "  Joint 1 position: " << controller->getJointPosition(1) << std::endl;
        }
    }

    // 获取视觉检测器服务
    auto* vision_plugin = manager.getPlugin("vision");
    if (vision_plugin) {
        auto* detector = static_cast<robot::VisionDetector*>(
            vision_plugin->getService("VisionDetector"));
        if (detector) {
            std::cout << "\nUsing VisionDetector:" << std::endl;
            std::vector<uint8_t> dummy_image(640 * 480 * 3, 0);
            auto detections = detector->detect(dummy_image);
            for (const auto& det : detections) {
                std::cout << "  - " << det.label
                          << " (confidence: " << std::fixed << std::setprecision(2) << det.confidence
                          << ") at (" << det.x << ", " << det.y << ")" << std::endl;
            }
        }
    }

    // Step 5: 使用事件系统
    printSeparator("Step 5: Event System");
    auto& event_bus = manager.getEventBus();

    // 订阅事件
    auto sub_id = event_bus.subscribe("robot.status", [](const std::string& event, const nlohmann::json& data) {
        std::cout << "Event received: " << event << std::endl;
        std::cout << "  Data: " << data.dump(2) << std::endl;
    });

    // 发布事件
    event_bus.publish("robot.status", {{"state", "moving"}, {"joint_positions", {1.57, -0.5, 0.0, 0.0, 0.0, 0.0}}});

    // 取消订阅
    event_bus.unsubscribe(sub_id);

    // Step 6: 停止并卸载插件
    printSeparator("Step 6: Stop and Unload Plugins");
    for (const auto& meta : available_plugins) {
        std::cout << "Stopping " << meta.name << "..." << std::endl;
        manager.stopPlugin(meta.name);
        std::cout << "Unloading " << meta.name << "..." << std::endl;
        manager.unloadPlugin(meta.name);
    }

    std::cout << "\nAll plugins unloaded. Exiting." << std::endl;
    return 0;
}
