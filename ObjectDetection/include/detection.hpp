#pragma once

#include <opencv2/core.hpp>

struct Detection {
    int class_id;
    float score;
    cv::Rect box;
};

struct GlobalAngles {
    float yaw;
    float pitch;
};

struct GlobalDetection {
    int class_id;
    float score;
    float yaw;
    float pitch;
    int view_id;
};
