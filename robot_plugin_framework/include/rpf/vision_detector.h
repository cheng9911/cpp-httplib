#pragma once

#include <string>
#include <vector>

namespace robot {

struct Detection {
    std::string label;
    double confidence;
    double x, y, width, height;
};

struct Pose {
    double x, y, z;
    double rx, ry, rz;
};

class VisionDetector {
public:
    virtual ~VisionDetector() = default;
    virtual std::vector<Detection> detect(const std::vector<uint8_t>& image) = 0;
};

class PoseEstimator {
public:
    virtual ~PoseEstimator() = default;
    virtual Pose estimatePose(const Detection& detection) = 0;
};

} // namespace robot
