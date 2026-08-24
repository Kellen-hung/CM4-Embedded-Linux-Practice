#include <opencv2/opencv.hpp>
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

constexpr double H_FOV = 100.0;
constexpr double V_FOV = 83.56;
constexpr double PI = 3.14159265358979323846;

constexpr int WARMUP = 5;
constexpr int RUNS = 30;


void buildMap(int src_w, int src_h, int out_w, int out_h, double yaw_deg, cv::Mat &map_x, cv::Mat &map_y)
{
    map_x.create(out_h, out_w, CV_32FC1);
    map_y.create(out_h, out_w, CV_32FC1);

    double yaw = yaw_deg * PI / 180.0;
    double h_fov = H_FOV * PI / 180.0;
    double v_fov = V_FOV * PI / 180.0;

    double tan_h = std::tan(h_fov / 2.0);
    double tan_v = std::tan(v_fov / 2.0);

    double cos_yaw = std::cos(yaw);
    double sin_yaw = std::sin(yaw);

    for (int y = 0; y < out_h; y++) {
        for (int x = 0; x < out_w; x++) {
            double nx = (2.0 * (x + 0.5) / out_w - 1.0) * tan_h;
            double ny = (2.0 * (y + 0.5) / out_h - 1.0) * tan_v;

            double rx = nx;
            double ry = ny;
            double rz = 1.0;

            double rotated_x = cos_yaw * rx + sin_yaw * rz;
            double rotated_z = -sin_yaw * rx + cos_yaw * rz;

            double length = std::sqrt(rotated_x * rotated_x + ry * ry + rotated_z * rotated_z);

            double longitude = std::atan2(rotated_x, rotated_z);
            double latitude = -std::asin(ry / length);

            double src_x = (longitude / (2.0 * PI) + 0.5) * src_w;
            double src_y = (0.5 - latitude / PI) * src_h;

            if (src_x < 0)
                src_x += src_w;

            if (src_x >= src_w)
                src_x -= src_w;

            map_x.at<float>(y, x) = static_cast<float>(src_x);
            map_y.at<float>(y, x) = static_cast<float>(src_y);
        }
    }
}


int main()
{
    cv::Mat image = cv::imread("raw.jpg");

    if (image.empty()) {
        std::cerr << "Failed to load raw.jpg\n";
        return 1;
    }

    std::cout << "Input: " << image.cols << "x" << image.rows << "\n";
    std::cout << "H_FOV: " << H_FOV << " degrees\n";
    std::cout << "V_FOV: " << V_FOV << " degrees\n";
    std::cout << "Yaw: 0 degrees\n\n";

    std::vector<cv::Size> sizes = {
        {1280, 960},
        {640, 480},
        {416, 312}
    };

    for (const auto &size : sizes) {
        cv::Mat map_x;
        cv::Mat map_y;
        cv::Mat output;

        std::cout << "Building map for " << size.width << "x" << size.height << "...\n";

        buildMap(image.cols, image.rows, size.width, size.height, 180.0, map_x, map_y);

        std::cout << "Warmup...\n";

        for (int i = 0; i < WARMUP; i++) {
            cv::remap(image, output, map_x, map_y, cv::INTER_LINEAR, cv::BORDER_WRAP);
        }

        double total_ms = 0.0;
        double min_ms = 1e9;
        double max_ms = 0.0;

        std::cout << "Benchmark...\n";

        for (int i = 0; i < RUNS; i++) {
            auto start = std::chrono::steady_clock::now();

            cv::remap(image, output, map_x, map_y, cv::INTER_LINEAR, cv::BORDER_WRAP);

            auto end = std::chrono::steady_clock::now();

            double ms = std::chrono::duration<double, std::milli>(end - start).count();

            total_ms += ms;
            min_ms = std::min(min_ms, ms);
            max_ms = std::max(max_ms, ms);
        }

        double avg_ms = total_ms / RUNS;

        std::cout << size.width << "x" << size.height
                  << "  avg=" << avg_ms << " ms"
                  << "  min=" << min_ms << " ms"
                  << "  max=" << max_ms << " ms\n";

        std::string filename = "yaw0_" + std::to_string(size.width) + "x" + std::to_string(size.height) + ".jpg";

        if (!cv::imwrite(filename, output)) {
            std::cerr << "Failed to save " << filename << "\n";
            return 1;
        }

        std::cout << "Saved: " << filename << "\n\n";
    }

    return 0;
}