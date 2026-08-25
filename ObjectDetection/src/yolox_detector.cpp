#include "yolox_detector.hpp"

#include "config.hpp"

#include <opencv2/imgproc.hpp>

#include <chrono>
#include <cmath>
#include <stdexcept>

namespace {
using Clock = std::chrono::steady_clock;
double elapsedMs(Clock::time_point start, Clock::time_point end)
{
    return std::chrono::duration<double, std::milli>(end - start).count();
}
}

YoloXDetector::YoloXDetector(const std::string& model_path)
    : net_(cv::dnn::readNetFromONNX(model_path)),
      canvas_(config::MODEL_HEIGHT, config::MODEL_WIDTH, CV_8UC3)
{
    if (net_.empty())
        throw std::runtime_error("Failed to load model: " + model_path);
    buildGrid();
}

void YoloXDetector::buildGrid()
{
    for (int stride : {8, 16, 32}) {
        const int width = config::MODEL_WIDTH / stride;
        const int height = config::MODEL_HEIGHT / stride;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                grid_x_.push_back(static_cast<float>(x));
                grid_y_.push_back(static_cast<float>(y));
                strides_.push_back(static_cast<float>(stride));
            }
        }
    }
}

DetectionResult YoloXDetector::detect(const cv::Mat& perspective_view)
{
    if (perspective_view.cols != config::VIEW_WIDTH || perspective_view.rows != config::VIEW_HEIGHT)
        throw std::invalid_argument("YOLOX expects a 416x312 perspective view");
    if (perspective_view.type() != CV_8UC3)
        throw std::invalid_argument("YOLOX expects a CV_8UC3 BGR perspective view");

    DetectionResult result;
    const auto preprocess_start = Clock::now();
    canvas_.setTo(cv::Scalar(114, 114, 114));
    perspective_view.copyTo(canvas_(cv::Rect(0, 0, config::VIEW_WIDTH, config::VIEW_HEIGHT)));
    cv::Mat blob;
    cv::dnn::blobFromImage(canvas_, blob, 1.0, cv::Size(config::MODEL_WIDTH, config::MODEL_HEIGHT),
                           cv::Scalar(), false, false);
    net_.setInput(blob);
    const auto preprocess_end = Clock::now();
    const auto inference_start = Clock::now();
    cv::Mat output = net_.forward();
    const auto inference_end = Clock::now();

    const auto postprocess_start = Clock::now();
    result.detections = postprocess(output);
    const auto postprocess_end = Clock::now();
    result.preprocess_ms = elapsedMs(preprocess_start, preprocess_end);
    result.inference_ms = elapsedMs(inference_start, inference_end);
    result.postprocess_ms = elapsedMs(postprocess_start, postprocess_end);
    return result;
}

std::vector<Detection> YoloXDetector::postprocess(const cv::Mat& output) const
{
    constexpr int CLASS_COUNT = 80;
    const int prediction_count = static_cast<int>(grid_x_.size());
    if (output.total() != static_cast<std::size_t>(prediction_count * (CLASS_COUNT + 5)))
        throw std::runtime_error("Unexpected YOLOX output shape");
    const cv::Mat predictions = output.reshape(1, prediction_count);
    std::vector<cv::Rect> boxes;
    std::vector<float> scores;
    std::vector<int> class_ids;

    for (int i = 0; i < predictions.rows; ++i) {
        const float* data = predictions.ptr<float>(i);
        int best_class = 0;
        float best_class_score = data[5];
        for (int class_id = 1; class_id < CLASS_COUNT; ++class_id) {
            if (data[5 + class_id] > best_class_score) {
                best_class_score = data[5 + class_id];
                best_class = class_id;
            }
        }
        const float score = data[4] * best_class_score;
        if (score < config::SCORE_THRESHOLD)
            continue;

        const float center_x = (data[0] + grid_x_[i]) * strides_[i];
        const float center_y = (data[1] + grid_y_[i]) * strides_[i];
        const float width = std::exp(data[2]) * strides_[i];
        const float height = std::exp(data[3]) * strides_[i];
        boxes.emplace_back(static_cast<int>(center_x - width / 2.0F),
                           static_cast<int>(center_y - height / 2.0F),
                           static_cast<int>(width), static_cast<int>(height));
        scores.push_back(score);
        class_ids.push_back(best_class);
    }

    std::vector<int> indices;
    cv::dnn::NMSBoxesBatched(boxes, scores, class_ids, config::SCORE_THRESHOLD,
                             config::NMS_THRESHOLD, indices);
    std::vector<Detection> detections;
    for (int index : indices) {
        cv::Rect box = boxes[index] & cv::Rect(0, 0, config::VIEW_WIDTH, config::VIEW_HEIGHT);
        if (box.width > 0 && box.height > 0)
            detections.push_back({class_ids[index], scores[index], box});
    }
    return detections;
}
