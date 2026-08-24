#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <iostream>
#include <cstdlib>
#include <chrono>
#include <csignal>

volatile sig_atomic_t running = 1;

void signal_handler(int sig)
{
    running = 0;
}

int main(int argc, char *argv[])
{
    /* Get frame with Gstreamer!! */
    // init 
    gst_init(&argc, &argv);

    struct sigaction sa = {};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, nullptr) == -1) {
        perror("sigaction");
        return 1;
    }

    const char *rtsp_url = std::getenv("RTSP_URL");

    if (rtsp_url == nullptr) {
        std::cerr << "RTSP_URL is not set" << std::endl;
        return 1;
    }

    // create pipeline
    std::string pipeline_str =
        "rtspsrc location=\"" + std::string(rtsp_url) + "\" latency=200 protocols=tcp ! "
        "application/x-rtp,media=video,encoding-name=H264 ! "
        "rtph264depay wait-for-keyframe=true request-keyframe=true ! "
        "video/x-h264,alignment=au ! "
        "h264parse ! "
        "avdec_h264 ! "
        "queue name=process_q max-size-buffers=1 max-size-bytes=0 max-size-time=0 ! "
        "videoconvert ! "
        "video/x-raw,format=BGR ! "
        "videoscale ! "
        "video/x-raw,format=BGR,width=1280,height=640 ! "
        "appsink name=sink";
        
    GError *error = nullptr;

    GstElement *pipeline = gst_parse_launch(
        pipeline_str.c_str(),
        &error
    );

    if (error != nullptr) {
        std::cerr << "Pipeline error: " << error->message << std::endl;
        g_error_free(error);
        return 1;
    }

    if (pipeline == nullptr) {
        std::cerr << "Failed to create pipeline" << std::endl;
        return 1;
    }

    std::cout << "Pipeline created successfully" << std::endl;

    // start pipeline
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);

    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "Failed to start pipeline" << std::endl;

        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);

        return 1;
    }

    std::cout << "Pipeline started" << std::endl;

    // get sample from appsink
    GstElement *sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");

    if (sink == nullptr) {
        std::cerr << "Failed to get appsink" << std::endl;

        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);

        return 1;
    }

    std::cout << "Waiting for sample..." << std::endl;

    int frame_count = 0;
    auto start_time = std::chrono::steady_clock::now();

    while (running) {
        GstSample *sample = gst_app_sink_try_pull_sample(
            GST_APP_SINK(sink),
            2 * GST_SECOND
        );

        if (sample == nullptr) {
            if (!running) {
                break;
            }

            std::cerr << "Failed to get sample" << std::endl;
            continue;
        }

        frame_count++;

        auto now = std::chrono::steady_clock::now();
        double elapsed =
            std::chrono::duration<double>(now - start_time).count();

        if (elapsed >= 1.0) {
            std::cout << "FPS: " << frame_count / elapsed << std::endl;

            frame_count = 0;
            start_time = now;
        }

        gst_sample_unref(sample);
    }

    std::cout << "\nStopping pipeline..." << std::endl;

    gst_object_unref(sink);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    return 0;
}