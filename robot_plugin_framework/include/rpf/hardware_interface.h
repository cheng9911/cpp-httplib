#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>

namespace rpf {

// 关节状态
struct JointState {
    std::string name;
    double position = 0.0;  // 位置 (rad)
    double velocity = 0.0;  // 速度 (rad/s)
    double effort = 0.0;    // 力矩 (Nm)
};

// 机器人状态
struct RobotState {
    std::vector<JointState> joints;
    double timestamp = 0.0;
    bool is_moving = false;
};

// 传感器数据
struct SensorData {
    std::string name;
    std::string type;  // "force", "torque", "position", "velocity", "imu"
    std::vector<double> values;
};

// 硬件接口
class Hardware {
public:
    virtual ~Hardware() = default;

    // 初始化硬件
    virtual bool initialize(const std::string& config_path = "") = 0;

    // 获取机器人状态
    virtual RobotState getState() = 0;

    // 设置关节位置
    virtual bool setJointPosition(const std::string& joint_name, double position) = 0;
    virtual bool setJointPositions(const std::map<std::string, double>& positions) = 0;

    // 设置关节速度
    virtual bool setJointVelocity(const std::string& joint_name, double velocity) = 0;
    virtual bool setJointVelocities(const std::map<std::string, double>& velocities) = 0;

    // 设置关节力矩
    virtual bool setJointEffort(const std::string& joint_name, double effort) = 0;
    virtual bool setJointEfforts(const std::map<std::string, double>& efforts) = 0;

    // 获取传感器数据
    virtual std::vector<SensorData> getSensorData() const = 0;

    // 执行一步仿真
    virtual bool stepSimulation(double dt = 0.001) = 0;

    // 重置仿真
    virtual bool reset() = 0;

    // 仿真控制
    virtual bool startSimulation() = 0;
    virtual bool stopSimulation() = 0;
    virtual bool isSimulating() const = 0;
};

// 仿真器接口
class Simulator {
public:
    virtual ~Simulator() = default;

    // 加载模型
    virtual bool loadModel(const std::string& model_path) = 0;

    // 获取模型信息
    virtual std::vector<std::string> getJointNames() const = 0;
    virtual std::vector<std::string> getBodyNames() const = 0;
    virtual std::vector<std::string> getSensorNames() const = 0;

    // 仿真参数
    virtual void setGravity(const std::vector<double>& gravity) = 0;
    virtual void setTimeStep(double dt) = 0;

    // 可视化
    virtual bool enableVisualization() = 0;
    virtual bool disableVisualization() = 0;
};

} // namespace rpf
