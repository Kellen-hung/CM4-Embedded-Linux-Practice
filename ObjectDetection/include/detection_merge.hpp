#pragma once

#include "detection.hpp"

#include <vector>

float normalizeYaw(float yaw);
float yawDistance(float a, float b);

std::vector<GlobalDetection> toGlobalDetections(
    const std::vector<Detection>& detections,
    int view_id,
    const std::vector<GlobalAngles>& angles);

std::vector<GlobalDetection> mergeDetections(const std::vector<GlobalDetection>& input);
