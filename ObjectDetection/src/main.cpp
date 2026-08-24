#include "config.hpp"
#include "detection_merge.hpp"
#include "dewarp.hpp"
#include "rtsp_capture.hpp"
#include "yolox_detector.hpp"

#include <opencv2/core.hpp>

#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {
volatile sig_atomic_t running = 1;
using Clock = std::chrono::steady_clock;

void signalHandler(int)
{
    running = 0;
}

double elapsedMs(Clock::time_point start, Clock::time_point end)
{
    return std::chrono::duration<double, std::milli>(end - start).count();
}

const char* className(int class_id)
{
    static constexpr const char* NAMES[] = {
        "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat", "traffic light",
        "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow",
        "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
        "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove", "skateboard", "surfboard",
        "tennis racket", "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple",
        "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch",
        "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse", "remote", "keyboard", "cell phone",
        "microwave", "oven", "toaster", "sink", "refrigerator", "book", "clock", "vase", "scissors", "teddy bear",
        "hair drier", "toothbrush"
    };
    return class_id >= 0 && class_id < 80 ? NAMES[class_id] : "unknown";
}

void printMerged(const std::vector<GlobalDetection>& raw)
{
    const auto merged = mergeDetections(raw);
    std::cout << "\n===== 360 MERGED RESULT =====\n";
    for (const auto& detection : merged) {
        std::cout << className(detection.class_id) << " score=" << detection.score
                  << " yaw=" << detection.yaw << "deg pitch=" << detection.pitch
                  << "deg source_view=" << detection.view_id << '\n';
    }
    std::cout << "Raw detections: " << raw.size() << " -> Merged: " << merged.size()
              << "\n=============================\n";
}
}  // namespace

int main(int argc, char* argv[])
{
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: " << argv[0] << " <RTSP_URL> [MODEL_PATH]\n";
        return 1;
    }

    try {
        struct sigaction action {};
        action.sa_handler = signalHandler;
        sigemptyset(&action.sa_mask);
        sigaction(SIGINT, &action, nullptr);
        sigaction(SIGTERM, &action, nullptr);

        cv::setNumThreads(config::OPENCV_THREADS);
        const std::string model_path = argc == 3 ? argv[2] : config::MODEL_PATH;
        std::cout << "OpenCV threads: " << cv::getNumThreads() << '\n'
                  << "Loading YOLOX-Nano: " << model_path << '\n';

        YoloXDetector detector(model_path);
        RtspCapture capture(argv[1], Decoder::Hardware);
        Dewarper dewarper;
        std::vector<GlobalDetection> cycle_detections;
        int current_view = 0;

        while (running) {
            RtspFrame frame = capture.pullFrame(200);
            if (!frame) {
                std::string error;
                if (capture.checkError(error)) {
                    std::cerr << "GStreamer: " << error << '\n';
                    return 1;
                }
                continue;
            }

            const cv::Mat& panorama = frame.image();
            if (dewarper.ensureMaps(panorama.cols, panorama.rows)) {
                std::cout << "RTSP frame: " << panorama.cols << 'x' << panorama.rows
                          << "; projection maps rebuilt for 4 views\n";
                current_view = 0;
                cycle_detections.clear();
            }

            const auto total_start = Clock::now();
            const auto remap_start = Clock::now();
            cv::Mat perspective = dewarper.dewarp(panorama, current_view);
            const auto remap_end = Clock::now();
            DetectionResult result = detector.detect(perspective);
            const auto total_end = Clock::now();

            std::vector<std::pair<float, float>> angles;
            angles.reserve(result.detections.size());
            for (const auto& detection : result.detections)
                angles.push_back(dewarper.globalAngles(detection.box, current_view));
            auto global = toGlobalDetections(result.detections, current_view, angles);
            cycle_detections.insert(cycle_detections.end(), global.begin(), global.end());

            std::cout << std::fixed << std::setprecision(2)
                      << "view=" << current_view << " yaw=" << dewarper.viewYaw(current_view)
                      << " remap=" << elapsedMs(remap_start, remap_end)
                      << " pre=" << result.preprocess_ms
                      << " infer=" << result.inference_ms
                      << " post=" << result.postprocess_ms
                      << " total=" << elapsedMs(total_start, total_end) << " ms";
            for (const auto& detection : global) {
                std::cout << "  [" << className(detection.class_id) << ' ' << detection.score
                          << " yaw=" << detection.yaw << "deg pitch=" << detection.pitch << "deg]";
            }
            std::cout << '\n';

            if (current_view == static_cast<int>(config::VIEW_YAWS.size()) - 1) {
                printMerged(cycle_detections);
                cycle_detections.clear();
            }
            current_view = (current_view + 1) % static_cast<int>(config::VIEW_YAWS.size());
        }

        std::cout << "Stopping cleanly...\n";
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
