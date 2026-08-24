#pragma once

#include <array>

namespace config {

inline constexpr int VIEW_WIDTH = 416;
inline constexpr int VIEW_HEIGHT = 312;
inline constexpr int MODEL_WIDTH = 416;
inline constexpr int MODEL_HEIGHT = 416;

inline constexpr float H_FOV = 100.0F;
inline constexpr float V_FOV = 83.56F;
inline constexpr float SCORE_THRESHOLD = 0.3F;
inline constexpr float NMS_THRESHOLD = 0.45F;
inline constexpr float MERGE_YAW_THRESHOLD = 5.0F;
inline constexpr float MERGE_PITCH_THRESHOLD = 5.0F;

inline constexpr int DETECTION_FPS = 5;
inline constexpr int OPENCV_THREADS = 4;
inline constexpr std::array<float, 4> VIEW_YAWS = {0.0F, 90.0F, 180.0F, -90.0F};
inline constexpr const char* MODEL_PATH = "models/yolox_nano.onnx";

}  // namespace config
