#include "detection_merge.hpp"

#include "config.hpp"

#include <cmath>
#include <stdexcept>

float normalizeYaw(float yaw)
{
    while (yaw >= 180.0F) yaw -= 360.0F;
    while (yaw < -180.0F) yaw += 360.0F;
    return yaw;
}

float yawDistance(float a, float b)
{
    return std::fabs(normalizeYaw(a - b));
}

std::vector<GlobalDetection> toGlobalDetections(
    const std::vector<Detection>& detections,
    int view_id,
    const std::vector<GlobalAngles>& angles)
{
    if (detections.size() != angles.size())
        throw std::invalid_argument("Detection and angle counts differ");
    std::vector<GlobalDetection> result;
    result.reserve(detections.size());
    for (std::size_t i = 0; i < detections.size(); ++i)
        result.push_back({detections[i].class_id, detections[i].score,
                          normalizeYaw(angles[i].yaw), angles[i].pitch, view_id});
    return result;
}

std::vector<GlobalDetection> mergeDetections(const std::vector<GlobalDetection>& input)
{
    std::vector<GlobalDetection> merged;
    for (const GlobalDetection& detection : input) {
        int duplicate_index = -1;
        for (int i = 0; i < static_cast<int>(merged.size()); ++i) {
            if (merged[i].class_id != detection.class_id)
                continue;
            if (merged[i].view_id == detection.view_id)
                continue;
            if (yawDistance(merged[i].yaw, detection.yaw) >= config::MERGE_YAW_THRESHOLD)
                continue;
            if (std::fabs(merged[i].pitch - detection.pitch) >= config::MERGE_PITCH_THRESHOLD)
                continue;
            duplicate_index = i;
            break;
        }
        if (duplicate_index == -1)
            merged.push_back(detection);
        else if (detection.score > merged[duplicate_index].score)
            merged[duplicate_index] = detection;
    }
    return merged;
}
