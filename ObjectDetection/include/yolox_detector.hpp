#pragma once

#include "detection.hpp"

#include <opencv2/dnn.hpp>

#include <string>
#include <vector>

struct DetectionResult {
    std::vector<Detection> detections;
    double preprocess_ms = 0.0;
    double inference_ms = 0.0;
    double postprocess_ms = 0.0;
};

class YoloXDetector {
public:
    explicit YoloXDetector(const std::string& model_path);
    DetectionResult detect(const cv::Mat& perspective_view);

private:
    void buildGrid();
    std::vector<Detection> postprocess(const cv::Mat& output) const;

    cv::dnn::Net net_;
    cv::Mat canvas_;
    std::vector<float> grid_x_;
    std::vector<float> grid_y_;
    std::vector<float> strides_;
};
