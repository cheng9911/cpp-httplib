#pragma once

#include <string>
#include <vector>

namespace robot {

class MotionController {
public:
    virtual ~MotionController() = default;
    virtual bool setJointPosition(int joint_id, double position) = 0;
    virtual double getJointPosition(int joint_id) const = 0;
    virtual int getJointCount() const = 0;
};

class TrajectoryPlanner {
public:
    virtual ~TrajectoryPlanner() = default;
    virtual std::vector<double> planTrajectory(
        const std::vector<double>& start,
        const std::vector<double>& end,
        double duration) = 0;
};

} // namespace robot
