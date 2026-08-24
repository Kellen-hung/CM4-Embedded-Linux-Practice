#include "dewarp.hpp"

#include "config.hpp"

#include <opencv2/imgproc.hpp>

#include <cmath>
#include <stdexcept>

namespace {
constexpr double PI = 3.14159265358979323846;
}

bool Dewarper::ensureMaps(int source_width, int source_height)
{
    if (source_width == source_width_ && source_height == source_height_)
        return false;

    if (source_width <= 0 || source_height <= 0)
        throw std::invalid_argument("Invalid source resolution");

    for (std::size_t i = 0; i < maps_.size(); ++i)
        buildMap(maps_[i], source_width, source_height, config::VIEW_YAWS[i]);

    source_width_ = source_width;
    source_height_ = source_height;
    return true;
}

void Dewarper::buildMap(DewarpMap& map, int source_width, int source_height, float yaw_deg) const
{
    map.yaw = yaw_deg;
    map.map_x.create(config::VIEW_HEIGHT, config::VIEW_WIDTH, CV_32FC1);
    map.map_y.create(config::VIEW_HEIGHT, config::VIEW_WIDTH, CV_32FC1);

    const double yaw = yaw_deg * PI / 180.0;
    const double tan_h = std::tan(config::H_FOV * PI / 360.0);
    const double tan_v = std::tan(config::V_FOV * PI / 360.0);
    const double cos_yaw = std::cos(yaw);
    const double sin_yaw = std::sin(yaw);

    for (int y = 0; y < config::VIEW_HEIGHT; ++y) {
        for (int x = 0; x < config::VIEW_WIDTH; ++x) {
            const double rx = (2.0 * (x + 0.5) / config::VIEW_WIDTH - 1.0) * tan_h;
            const double ry = (2.0 * (y + 0.5) / config::VIEW_HEIGHT - 1.0) * tan_v;
            const double rz = 1.0;
            const double rotated_x = cos_yaw * rx + sin_yaw * rz;
            const double rotated_z = -sin_yaw * rx + cos_yaw * rz;
            const double length = std::sqrt(rotated_x * rotated_x + ry * ry + rotated_z * rotated_z);
            const double longitude = std::atan2(rotated_x, rotated_z);
            const double latitude = -std::asin(ry / length);
            double source_x = (longitude / (2.0 * PI) + 0.5) * source_width;

            if (source_x < 0.0)
                source_x += source_width;
            if (source_x >= source_width)
                source_x -= source_width;

            map.map_x.at<float>(y, x) = static_cast<float>(source_x);
            map.map_y.at<float>(y, x) = static_cast<float>((0.5 - latitude / PI) * source_height);
        }
    }
}

cv::Mat Dewarper::dewarp(const cv::Mat& source, int view_id) const
{
    if (source.cols != source_width_ || source.rows != source_height_)
        throw std::runtime_error("Dewarp maps do not match source resolution");
    if (view_id < 0 || view_id >= static_cast<int>(maps_.size()))
        throw std::out_of_range("Invalid view id");

    cv::Mat output;
    cv::remap(source, output, maps_[view_id].map_x, maps_[view_id].map_y,
              cv::INTER_LINEAR, cv::BORDER_WRAP);
    return output;
}

std::pair<float, float> Dewarper::globalAngles(const cv::Rect& box, int view_id) const
{
    const double center_x = box.x + box.width / 2.0;
    const double center_y = box.y + box.height / 2.0;
    const double normalized_x = 2.0 * center_x / config::VIEW_WIDTH - 1.0;
    const double local_yaw = std::atan(normalized_x * std::tan(config::H_FOV * PI / 360.0)) * 180.0 / PI;
    double yaw = maps_.at(view_id).yaw + local_yaw;
    while (yaw >= 180.0) yaw -= 360.0;
    while (yaw < -180.0) yaw += 360.0;

    const double nx = normalized_x * std::tan(config::H_FOV * PI / 360.0);
    const double ny = (2.0 * center_y / config::VIEW_HEIGHT - 1.0) * std::tan(config::V_FOV * PI / 360.0);
    const double length = std::sqrt(nx * nx + ny * ny + 1.0);
    const double pitch = -std::asin(ny / length) * 180.0 / PI;
    return {static_cast<float>(yaw), static_cast<float>(pitch)};
}

float Dewarper::viewYaw(int view_id) const
{
    return maps_.at(view_id).yaw;
}
