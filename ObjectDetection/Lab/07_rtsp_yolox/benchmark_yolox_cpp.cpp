#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>

#include <chrono>
#include <iostream>

constexpr int MODEL_SIZE = 416;
constexpr int VIEW_W = 416;
constexpr int VIEW_H = 312;

constexpr int WARMUP = 5;
constexpr int RUNS = 30;

int main()
{
    cv::dnn::Net net = cv::dnn::readNetFromONNX("../06_model_comparison/models/yolox-nano/yolox_nano.onnx");

    if (net.empty()) {
        std::cerr << "Failed to load model\n";
        return 1;
    }

    cv::Mat image = cv::imread("yaw0_416x312.jpg");

    if (image.empty()) {
        std::cerr << "Failed to load image\n";
        return 1;
    }

    cv::Mat canvas(MODEL_SIZE, MODEL_SIZE, CV_8UC3, cv::Scalar(114, 114, 114));
    image.copyTo(canvas(cv::Rect(0, 0, VIEW_W, VIEW_H)));

    cv::Mat blob;
    cv::dnn::blobFromImage(canvas, blob, 1.0, cv::Size(MODEL_SIZE, MODEL_SIZE), cv::Scalar(), false, false);

    std::cout << "OpenCV: " << CV_VERSION << "\n";
    std::cout << "Threads: " << cv::getNumThreads() << "\n";
    std::cout << "Warmup...\n";

    for (int i = 0; i < WARMUP; i++) {
        net.setInput(blob);
        net.forward();
    }

    double total = 0.0;
    double min_ms = 1e9;
    double max_ms = 0.0;

    std::cout << "Benchmark...\n";

    for (int i = 0; i < RUNS; i++) {
        auto start = std::chrono::steady_clock::now();

        net.setInput(blob);
        net.forward();

        auto end = std::chrono::steady_clock::now();

        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        total += ms;
        min_ms = std::min(min_ms, ms);
        max_ms = std::max(max_ms, ms);

        std::cout << i + 1 << ": " << ms << " ms\n";
    }

    double avg = total / RUNS;

    std::cout << "\n";
    std::cout << "Average : " << avg << " ms\n";
    std::cout << "Min     : " << min_ms << " ms\n";
    std::cout << "Max     : " << max_ms << " ms\n";
    std::cout << "FPS     : " << 1000.0 / avg << "\n";

    return 0;
}