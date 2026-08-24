#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <cstdlib>
#include <chrono>
#include <csignal>
#include <iomanip>
#include <sstream>

volatile sig_atomic_t running = 1;

void signal_handler(int sig)
{
    (void)sig;
    running = 0;
}

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);

    struct sigaction sa = {};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, nullptr) == -1) {
        perror("sigaction");
        return 1;
    }

    const char *rtsp_url = std::getenv("RTSP_URL");

    if (rtsp_url == nullptr) {
        std::cerr << "RTSP_URL is not set" << std::endl;
        return 1;
    }

    std::string pipeline_str =
        "rtspsrc location=\"" + std::string(rtsp_url) + "\" latency=200 protocols=tcp ! "
        "application/x-rtp,media=video,encoding-name=H264 ! "
        "rtph264depay wait-for-keyframe=true request-keyframe=true ! "
        "video/x-h264,alignment=au ! "
        "h264parse ! "
        "avdec_h264 ! "
        "queue max-size-buffers=1 max-size-bytes=0 max-size-time=0 ! "
        "videoconvert ! "
        "video/x-raw,format=BGR ! "
        "appsink name=sink max-buffers=1 drop=true sync=false";

    GError *error = nullptr;
    GstElement *pipeline = gst_parse_launch(pipeline_str.c_str(), &error);

    if (error != nullptr) {
        std::cerr << "Pipeline error: " << error->message << std::endl;
        g_error_free(error);
        return 1;
    }

    GstElement *sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");

    if (sink == nullptr) {
        std::cerr << "Failed to get appsink" << std::endl;
        gst_object_unref(pipeline);
        return 1;
    }

    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "Failed to start pipeline" << std::endl;
        gst_object_unref(sink);
        gst_object_unref(pipeline);
        return 1;
    }

    std::cout << "Collecting calibration frames..." << std::endl;

    const int target_count = 100;
    const auto interval = std::chrono::seconds(5);

    int saved_count = 0;
    auto next_save_time = std::chrono::steady_clock::now();

    while (running && saved_count < target_count) {
        GstSample *sample = gst_app_sink_try_pull_sample(GST_APP_SINK(sink), 200 * GST_MSECOND);

        if (sample == nullptr) {
            continue;
        }

        auto now = std::chrono::steady_clock::now();

        if (now >= next_save_time) {
            GstBuffer *buffer = gst_sample_get_buffer(sample);

            if (buffer != nullptr) {
                GstMapInfo map;

                if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
                    cv::Mat frame(1920, 3840, CV_8UC3, map.data);

                    std::ostringstream filename;
                    filename << "raw/raw_" << std::setw(3) << std::setfill('0') << saved_count << ".jpg";

                    if (cv::imwrite(filename.str(), frame)) {
                        std::cout << "Saved " << filename.str() << " (" << saved_count + 1 << "/" << target_count << ")" << std::endl;
                        saved_count++;
                    } else {
                        std::cerr << "Failed to save " << filename.str() << std::endl;
                    }

                    gst_buffer_unmap(buffer, &map);
                }
            }

            next_save_time = now + interval;
        }

        gst_sample_unref(sample);
    }

    std::cout << "Collected " << saved_count << " frames" << std::endl;
    std::cout << "Stopping pipeline..." << std::endl;

    gst_object_unref(sink);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    return 0;
}