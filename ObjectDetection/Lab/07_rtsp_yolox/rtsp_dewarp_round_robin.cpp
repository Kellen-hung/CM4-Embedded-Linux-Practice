#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#include <opencv2/opencv.hpp>

#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>
#include <string>
#include <vector>

constexpr int OUT_W = 416;
constexpr int OUT_H = 312;

constexpr double H_FOV = 100.0;
constexpr double V_FOV = 83.56;
constexpr double PI = 3.14159265358979323846;

volatile sig_atomic_t running = 1;

void signalHandler(int)
{
    running = 0;
}

void buildMap(int src_w, int src_h, double yaw_deg, cv::Mat &map_x, cv::Mat &map_y)
{
    map_x.create(OUT_H, OUT_W, CV_32FC1);
    map_y.create(OUT_H, OUT_W, CV_32FC1);

    double yaw = yaw_deg * PI / 180.0;
    double h_fov = H_FOV * PI / 180.0;
    double v_fov = V_FOV * PI / 180.0;

    double tan_h = std::tan(h_fov / 2.0);
    double tan_v = std::tan(v_fov / 2.0);

    double cos_yaw = std::cos(yaw);
    double sin_yaw = std::sin(yaw);

    for (int y = 0; y < OUT_H; y++) {
        for (int x = 0; x < OUT_W; x++) {
            double nx = (2.0 * (x + 0.5) / OUT_W - 1.0) * tan_h;
            double ny = (2.0 * (y + 0.5) / OUT_H - 1.0) * tan_v;

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

int main(int argc, char *argv[])
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <RTSP_URL>\n";
        return 1;
    }

    struct sigaction sa {};
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);

    gst_init(&argc, &argv);

    std::string pipeline_desc =
        "rtspsrc location=" + std::string(argv[1]) + " latency=0 drop-on-latency=true protocols=tcp ! "
        "application/x-rtp,media=video,encoding-name=H264 ! "
        "rtph264depay wait-for-keyframe=true request-keyframe=true ! "
        "video/x-h264,alignment=au ! "
        "h264parse ! "
        "avdec_h264 ! "
        "queue max-size-buffers=1 leaky=downstream ! "
        "videoconvert ! "
        "video/x-raw,format=BGR ! "
        "appsink name=sink max-buffers=1 drop=true sync=false";

    GError *error = nullptr;
    GstElement *pipeline = gst_parse_launch(pipeline_desc.c_str(), &error);

    if (!pipeline) {
        std::cerr << "Failed to create pipeline: " << (error ? error->message : "unknown error") << "\n";
        if (error)
            g_error_free(error);
        return 1;
    }

    GstElement *sink_element = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
    GstAppSink *sink = GST_APP_SINK(sink_element);

    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    std::vector<double> yaws = {0.0, 90.0, 180.0, -90.0};
    std::vector<cv::Mat> map_xs(4), map_ys(4);

    bool maps_ready = false;
    bool saved[4] = {false, false, false, false};
    int current_view = 0;
    uint64_t frame_count = 0;

    while (running) {
        GstSample *sample = gst_app_sink_try_pull_sample(sink, 200 * GST_MSECOND);

        if (!sample)
            continue;

        GstCaps *caps = gst_sample_get_caps(sample);
        GstBuffer *buffer = gst_sample_get_buffer(sample);

        GstVideoInfo info;

        if (!gst_video_info_from_caps(&info, caps)) {
            std::cerr << "Failed to read video info\n";
            gst_sample_unref(sample);
            continue;
        }

        int width = GST_VIDEO_INFO_WIDTH(&info);
        int height = GST_VIDEO_INFO_HEIGHT(&info);
        int stride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);

        if (!maps_ready) {
            std::cout << "RTSP frame: " << width << "x" << height << "\n";
            std::cout << "Building 4 projection maps...\n";

            for (int i = 0; i < 4; i++)
                buildMap(width, height, yaws[i], map_xs[i], map_ys[i]);

            maps_ready = true;

            std::cout << "Maps ready\n\n";
        }

        GstMapInfo map;

        if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
            gst_sample_unref(sample);
            continue;
        }

        cv::Mat frame(height, width, CV_8UC3, map.data, stride);
        cv::Mat dewarped;

        auto start = std::chrono::steady_clock::now();

        cv::remap(frame, dewarped, map_xs[current_view], map_ys[current_view], cv::INTER_LINEAR, cv::BORDER_WRAP);

        auto end = std::chrono::steady_clock::now();

        double remap_ms = std::chrono::duration<double, std::milli>(end - start).count();

        std::cout << "frame=" << frame_count << "  view=" << current_view << "  yaw=" << yaws[current_view] << "  remap=" << remap_ms << " ms\n";

        if (!saved[current_view]) {
            std::string filename = "rtsp_yaw_" + std::to_string(static_cast<int>(yaws[current_view])) + ".jpg";
            cv::imwrite(filename, dewarped);
            saved[current_view] = true;

            std::cout << "Saved " << filename << "\n";
        }

        current_view = (current_view + 1) % 4;
        frame_count++;

        gst_buffer_unmap(buffer, &map);
        gst_sample_unref(sample);
    }

    std::cout << "\nStopping...\n";

    gst_element_set_state(pipeline, GST_STATE_NULL);

    gst_object_unref(sink_element);
    gst_object_unref(pipeline);

    return 0;
}