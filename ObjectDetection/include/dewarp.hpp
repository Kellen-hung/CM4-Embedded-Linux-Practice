#pragma once

#include "detection.hpp"

#include <opencv2/core.hpp>

#include <array>
#include <utility>

struct DewarpMap {
    cv::Mat map_x;
    cv::Mat map_y;
    float yaw;
};

class Dewarper {
public:
    bool ensureMaps(int source_width, int source_height);
    cv::Mat dewarp(const cv::Mat& source, int view_id) const;
    std::pair<float, float> globalAngles(const cv::Rect& box, int view_id) const;
    float viewYaw(int view_id) const;

private:
    void buildMap(DewarpMap& map, int source_width, int source_height, float yaw) const;

    int source_width_ = 0;
    int source_height_ = 0;
    std::array<DewarpMap, 4> maps_;
};
