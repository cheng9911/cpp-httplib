#include "rpf/plugin_interface.h"
#include "rpf/motion_controller.h"
#include <iostream>
#include <vector>
#include <cmath>

namespace robot {

class MotionControlPlugin : public rpf::IPlugin,
                            public MotionController,
                            public TrajectoryPlanner {
public:
    MotionControlPlugin() : state_(rpf::PluginState::Unloaded) {}

    bool initialize() override {
        std::cout << "[MotionControl] Initializing..." << std::endl;
        joint_positions_.resize(6, 0.0);  // 6轴机器人
        state_ = rpf::PluginState::Initialized;
        return true;
    }

    bool start() override {
        std::cout << "[MotionControl] Starting..." << std::endl;
        state_ = rpf::PluginState::Running;
        return true;
    }

    bool stop() override {
        std::cout << "[MotionControl] Stopping..." << std::endl;
        state_ = rpf::PluginState::Stopped;
        return true;
    }

    void unload() override {
        std::cout << "[MotionControl] Unloading..." << std::endl;
        joint_positions_.clear();
        state_ = rpf::PluginState::Unloaded;
    }

    rpf::PluginState getState() const override { return state_; }
    std::string getName() const override { return "motion_control"; }
    std::string getVersion() const override { return "1.0.0"; }

    void* getService(const std::string& name) override {
        if (name == "MotionController") return static_cast<MotionController*>(this);
        if (name == "TrajectoryPlanner") return static_cast<TrajectoryPlanner*>(this);
        return nullptr;
    }

    // MotionController 接口实现
    bool setJointPosition(int joint_id, double position) override {
        if (joint_id < 0 || joint_id >= static_cast<int>(joint_positions_.size())) {
            return false;
        }
        joint_positions_[joint_id] = position;
        std::cout << "[MotionControl] Joint " << joint_id << " -> " << position << std::endl;
        return true;
    }

    double getJointPosition(int joint_id) const override {
        if (joint_id < 0 || joint_id >= static_cast<int>(joint_positions_.size())) {
            return 0.0;
        }
        return joint_positions_[joint_id];
    }

    int getJointCount() const override {
        return static_cast<int>(joint_positions_.size());
    }

    // TrajectoryPlanner 接口实现
    std::vector<double> planTrajectory(
        const std::vector<double>& start,
        const std::vector<double>& end,
        double duration) override {

        std::vector<double> result;
        int steps = static_cast<int>(duration * 100);  // 100Hz
        for (int i = 0; i <= steps; ++i) {
            double t = static_cast<double>(i) / steps;
            // 简单线性插值
            double pos = start[0] + (end[0] - start[0]) * t;
            result.push_back(pos);
        }
        return result;
    }

private:
    rpf::PluginState state_;
    std::vector<double> joint_positions_;
};

} // namespace robot

RPF_PLUGIN_EXPORT(robot::MotionControlPlugin)
