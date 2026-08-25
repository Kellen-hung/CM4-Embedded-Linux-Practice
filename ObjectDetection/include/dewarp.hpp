#pragma once

#include "config.hpp"
#include "detection.hpp"

#include <opencv2/core.hpp>

#include <array>

struct DewarpMap {
    cv::Mat map_x;
    cv::Mat map_y;
    float yaw = 0.0F;
};

class Dewarper {
    public:
        bool ensureMaps(int source_width, int source_height);
        cv::Mat dewarp(const cv::Mat& source, int view_id) const;
        GlobalAngles globalAngles(const cv::Rect& box, int view_id) const;
        float viewYaw(int view_id) const;

    private:
        void buildMap(DewarpMap& map, int source_width, int source_height, float yaw) const;

        int source_width_ = 0;
        int source_height_ = 0;
        std::array<DewarpMap, config::VIEW_COUNT> maps_;
};
