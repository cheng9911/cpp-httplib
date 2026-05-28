#include "rpf/plugin_interface.h"
#include "rpf/hardware_interface.h"
#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <cmath>
#include <mutex>
#include <thread>
#include <chrono>

namespace robot {

class MujocoHardwarePlugin : public rpf::IPlugin,
                             public rpf::Hardware,
                             public rpf::Simulator {
public:
    MujocoHardwarePlugin() : state_(rpf::PluginState::Unloaded),
                             simulating_(false),
                             time_step_(0.001),
                             time_(0.0) {}

    // IPlugin 接口
    bool initialize() override {
        std::cout << "[MuJoCo] Initializing hardware abstraction layer..." << std::endl;

        // 初始化默认关节
        joint_names_ = {"joint1", "joint2", "joint3", "joint4", "joint5", "joint6"};
        body_names_ = {"base_link", "link1", "link2", "link3", "link4", "link5", "link6"};
        sensor_names_ = {"force_sensor", "torque_sensor", "imu"};

        for (const auto& name : joint_names_) {
            joint_states_[name] = {name, 0.0, 0.0, 0.0};
            joint_targets_[name] = 0.0;
        }

        state_ = rpf::PluginState::Initialized;
        std::cout << "[MuJoCo] Initialized with " << joint_names_.size() << " joints" << std::endl;
        return true;
    }

    bool start() override {
        std::cout << "[MuJoCo] Starting simulation..." << std::endl;
        state_ = rpf::PluginState::Running;
        return true;
    }

    bool stop() override {
        std::cout << "[MuJoCo] Stopping simulation..." << std::endl;
        stopSimulation();
        state_ = rpf::PluginState::Stopped;
        return true;
    }

    void unload() override {
        std::cout << "[MuJoCo] Unloading..." << std::endl;
        stopSimulation();
        joint_states_.clear();
        joint_targets_.clear();
        joint_names_.clear();
        body_names_.clear();
        sensor_names_.clear();
        state_ = rpf::PluginState::Unloaded;
    }

    rpf::PluginState getState() const override { return state_; }
    std::string getName() const override { return "mujoco_hardware"; }
    std::string getVersion() const override { return "1.0.0"; }

    void* getService(const std::string& name) override {
        if (name == "Hardware") return static_cast<rpf::Hardware*>(this);
        if (name == "Simulator") return static_cast<rpf::Simulator*>(this);
        return nullptr;
    }

    // Hardware 接口实现
    bool initialize(const std::string& config_path) override {
        std::cout << "[MuJoCo] Loading configuration from: " << config_path << std::endl;
        // 在实际实现中，这里会加载MuJoCo模型文件
        return true;
    }

    rpf::RobotState getState() override {
        std::lock_guard<std::mutex> lock(mutex_);
        rpf::RobotState state;
        state.timestamp = time_;
        state.is_moving = simulating_;

        for (const auto& name : joint_names_) {
            auto it = joint_states_.find(name);
            if (it != joint_states_.end()) {
                state.joints.push_back(it->second);
            }
        }
        return state;
    }

    bool setJointPosition(const std::string& joint_name, double position) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (joint_targets_.find(joint_name) == joint_targets_.end()) {
            return false;
        }
        joint_targets_[joint_name] = position;
        std::cout << "[MuJoCo] Set joint " << joint_name << " target: " << position << std::endl;
        return true;
    }

    bool setJointPositions(const std::map<std::string, double>& positions) override {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [name, pos] : positions) {
            if (joint_targets_.find(name) != joint_targets_.end()) {
                joint_targets_[name] = pos;
            }
        }
        return true;
    }

    bool setJointVelocity(const std::string& joint_name, double velocity) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (joint_states_.find(joint_name) == joint_states_.end()) {
            return false;
        }
        joint_states_[joint_name].velocity = velocity;
        return true;
    }

    bool setJointVelocities(const std::map<std::string, double>& velocities) override {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [name, vel] : velocities) {
            if (joint_states_.find(name) != joint_states_.end()) {
                joint_states_[name].velocity = vel;
            }
        }
        return true;
    }

    bool setJointEffort(const std::string& joint_name, double effort) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (joint_states_.find(joint_name) == joint_states_.end()) {
            return false;
        }
        joint_states_[joint_name].effort = effort;
        return true;
    }

    bool setJointEfforts(const std::map<std::string, double>& efforts) override {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [name, eff] : efforts) {
            if (joint_states_.find(name) != joint_states_.end()) {
                joint_states_[name].effort = eff;
            }
        }
        return true;
    }

    std::vector<rpf::SensorData> getSensorData() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<rpf::SensorData> sensors;

        // 模拟力传感器数据
        rpf::SensorData force_sensor;
        force_sensor.name = "force_sensor";
        force_sensor.type = "force";
        force_sensor.values = {0.0, 0.0, 9.81};  // 模拟重力
        sensors.push_back(force_sensor);

        // 模拟力矩传感器数据
        rpf::SensorData torque_sensor;
        torque_sensor.name = "torque_sensor";
        torque_sensor.type = "torque";
        torque_sensor.values = {0.1, 0.2, 0.3};
        sensors.push_back(torque_sensor);

        // 模拟IMU数据
        rpf::SensorData imu;
        imu.name = "imu";
        imu.type = "imu";
        imu.values = {0.0, 0.0, 9.81, 0.0, 0.0, 0.0};  // 加速度 + 角速度
        sensors.push_back(imu);

        return sensors;
    }

    bool stepSimulation(double dt) override {
        std::lock_guard<std::mutex> lock(mutex_);

        // 仿真步进：使用低增益PD控制，增加阻尼使运动更平滑
        for (auto& [name, state] : joint_states_) {
            auto target_it = joint_targets_.find(name);
            if (target_it != joint_targets_.end()) {
                double error = target_it->second - state.position;
                // 极低增益PD控制：Kp=0.1, Kd=0.5 (更高阻尼)
                double Kp = 0.1;
                double Kd = 0.5;
                state.effort = Kp * error - Kd * state.velocity;
                state.velocity += state.effort * dt;
                state.position += state.velocity * dt;
            }
        }

        time_ += dt;
        return true;
    }

    bool reset() override {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [name, state] : joint_states_) {
            state.position = 0.0;
            state.velocity = 0.0;
            state.effort = 0.0;
            joint_targets_[name] = 0.0;
        }
        time_ = 0.0;
        std::cout << "[MuJoCo] Simulation reset" << std::endl;
        return true;
    }

    bool startSimulation() override {
        if (simulating_) {
            return true;
        }
        simulating_ = true;
        // 使用较慢的仿真频率 (2Hz)，步长0.5秒
        simulation_thread_ = std::thread([this]() {
            std::cout << "[MuJoCo] Simulation thread started (2Hz)" << std::endl;
            while (simulating_) {
                stepSimulation(0.5);  // 0.5秒步长
                std::this_thread::sleep_for(std::chrono::milliseconds(500));  // 500ms = 2Hz
            }
            std::cout << "[MuJoCo] Simulation thread stopped" << std::endl;
        });
        return true;
    }

    bool stopSimulation() override {
        if (!simulating_) {
            return true;
        }
        simulating_ = false;
        if (simulation_thread_.joinable()) {
            simulation_thread_.join();
        }
        return true;
    }

    bool isSimulating() const override {
        return simulating_;
    }

    // Simulator 接口实现
    bool loadModel(const std::string& model_path) override {
        std::cout << "[MuJoCo] Loading model from: " << model_path << std::endl;
        // 在实际实现中，这里会加载MuJoCo的XML模型文件
        return true;
    }

    std::vector<std::string> getJointNames() const override {
        return joint_names_;
    }

    std::vector<std::string> getBodyNames() const override {
        return body_names_;
    }

    std::vector<std::string> getSensorNames() const override {
        return sensor_names_;
    }

    void setGravity(const std::vector<double>& gravity) override {
        std::cout << "[MuJoCo] Setting gravity: ["
                  << gravity[0] << ", " << gravity[1] << ", " << gravity[2] << "]" << std::endl;
    }

    void setTimeStep(double dt) override {
        time_step_ = dt;
        std::cout << "[MuJoCo] Setting time step: " << dt << std::endl;
    }

    bool enableVisualization() override {
        std::cout << "[MuJoCo] Visualization enabled" << std::endl;
        return true;
    }

    bool disableVisualization() override {
        std::cout << "[MuJoCo] Visualization disabled" << std::endl;
        return true;
    }

private:
    rpf::PluginState state_;
    bool simulating_;
    double time_step_;
    double time_;

    std::vector<std::string> joint_names_;
    std::vector<std::string> body_names_;
    std::vector<std::string> sensor_names_;

    std::map<std::string, rpf::JointState> joint_states_;
    std::map<std::string, double> joint_targets_;

    std::thread simulation_thread_;
    mutable std::mutex mutex_;
};

} // namespace robot

RPF_PLUGIN_EXPORT(robot::MujocoHardwarePlugin)
