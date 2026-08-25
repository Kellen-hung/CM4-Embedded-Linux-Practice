#include "config.hpp"
#include "detection_merge.hpp"
#include "dewarp.hpp"
#include "rtsp_capture.hpp"
#include "yolox_detector.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

// anything in namespace will not be seen by other .cpp
namespace {
volatile sig_atomic_t running = 1;
using Clock = std::chrono::steady_clock;

// secure signal processing
void signalHandler(int)
{
    running = 0;
}

// benchmark
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
    if (class_id < 0 || class_id >= static_cast<int>(std::size(NAMES)))
        return "unknown";
    return NAMES[class_id];
}

void saveDebugImage(cv::Mat& perspective, const DetectionResult& result, int view_id)
{
    for (const auto& detection : result.detections) {
        cv::rectangle(perspective, detection.box, cv::Scalar(0, 255, 0), 2);

        const std::string label = std::string(className(detection.class_id)) + " " +
                                  cv::format("%.2f", detection.score);
        const cv::Point label_origin(detection.box.x,
                                     std::max(detection.box.y - 5, 12));
        cv::putText(perspective, label, label_origin, cv::FONT_HERSHEY_SIMPLEX,
                    0.4, cv::Scalar(0, 255, 0), 1, cv::LINE_AA);
    }

    const std::filesystem::path output_path =
        std::filesystem::path("debug_output") / ("view_" + std::to_string(view_id) + ".jpg");
    if (!cv::imwrite(output_path.string(), perspective))
        throw std::runtime_error("Failed to write debug image: " + output_path.string());
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

        // Ctrl + C
        sigaction(SIGINT, &action, nullptr);
        // systemd kill
        sigaction(SIGTERM, &action, nullptr);

        // multi thread
        cv::setNumThreads(config::OPENCV_THREADS);

        const std::string model_path = (argc == 3 ? argv[2] : config::MODEL_PATH);
        std::cout << "OpenCV threads: " << cv::getNumThreads() << '\n' << "Loading YOLOX-Nano: " << model_path << '\n';

        // class init (constructor)
        YoloXDetector detector(model_path);
        RtspCapture capture(argv[1], Decoder::Hardware);
        Dewarper dewarper;

        if constexpr (config::SAVE_DEBUG_IMAGES)
            std::filesystem::create_directories("debug_output");

        std::vector<GlobalDetection> cycle_detections;
        int current_view = 0;

        while (running) {
            // timeout = 200 (ms)
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

            // for Dynamic resolution
            if (dewarper.ensureMaps(panorama.cols, panorama.rows)) {
                std::cout << "RTSP frame: " << panorama.cols << 'x' << panorama.rows
                          << "; projection maps rebuilt for 4 views\n";
                current_view = 0;
                cycle_detections.clear();
            }

            const auto processing_start = Clock::now();

            const auto remap_start = Clock::now();
            cv::Mat perspective = dewarper.dewarp(panorama, current_view);
            const auto remap_end = Clock::now();

            DetectionResult result = detector.detect(perspective);
            const auto processing_end = Clock::now();

            if constexpr (config::SAVE_DEBUG_IMAGES)
                saveDebugImage(perspective, result, current_view);

            // projection inverse
            std::vector<GlobalAngles> angles;
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
                      << " processing=" << elapsedMs(processing_start, processing_end) << " ms";

            for (const auto& detection : global) {
                std::cout << "  [" << className(detection.class_id) << ' ' << detection.score
                          << " yaw=" << detection.yaw << "deg pitch=" << detection.pitch << "deg]";
            }
            std::cout << '\n';

            // merge
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
