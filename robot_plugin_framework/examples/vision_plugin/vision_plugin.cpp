#include "rpf/plugin_interface.h"
#include "rpf/vision_detector.h"
#include <iostream>
#include <vector>
#include <string>

namespace robot {

class VisionPlugin : public rpf::IPlugin,
                     public VisionDetector,
                     public PoseEstimator {
public:
    VisionPlugin() : state_(rpf::PluginState::Unloaded) {}

    bool initialize() override {
        std::cout << "[Vision] Initializing..." << std::endl;
        state_ = rpf::PluginState::Initialized;
        return true;
    }

    bool start() override {
        std::cout << "[Vision] Starting..." << std::endl;
        state_ = rpf::PluginState::Running;
        return true;
    }

    bool stop() override {
        std::cout << "[Vision] Stopping..." << std::endl;
        state_ = rpf::PluginState::Stopped;
        return true;
    }

    void unload() override {
        std::cout << "[Vision] Unloading..." << std::endl;
        state_ = rpf::PluginState::Unloaded;
    }

    rpf::PluginState getState() const override { return state_; }
    std::string getName() const override { return "vision"; }
    std::string getVersion() const override { return "1.0.0"; }

    void* getService(const std::string& name) override {
        if (name == "VisionDetector") return static_cast<VisionDetector*>(this);
        if (name == "PoseEstimator") return static_cast<PoseEstimator*>(this);
        return nullptr;
    }

    // VisionDetector 接口实现
    std::vector<Detection> detect(const std::vector<uint8_t>& /*image*/) override {
        // 模拟检测结果
        std::vector<Detection> results;
        results.push_back({"object_1", 0.95, 100.0, 100.0, 50.0, 50.0});
        results.push_back({"object_2", 0.87, 200.0, 150.0, 60.0, 40.0});
        std::cout << "[Vision] Detected " << results.size() << " objects" << std::endl;
        return results;
    }

    // PoseEstimator 接口实现
    Pose estimatePose(const Detection& detection) override {
        // 模拟位姿估计
        Pose pose;
        pose.x = detection.x * 0.001;  // 像素转米
        pose.y = detection.y * 0.001;
        pose.z = 0.5;
        pose.rx = 0.0;
        pose.ry = 0.0;
        pose.rz = 0.0;
        std::cout << "[Vision] Estimated pose for " << detection.label << std::endl;
        return pose;
    }

private:
    rpf::PluginState state_;
};

} // namespace robot

RPF_PLUGIN_EXPORT(robot::VisionPlugin)
