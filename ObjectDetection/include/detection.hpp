#pragma once

#include <opencv2/core.hpp>

struct Detection {
    int class_id;
    float score;
    cv::Rect box;
};

struct GlobalDetection {
    int class_id;
    float score;
    float yaw;
    float pitch;
    int view_id;
};
