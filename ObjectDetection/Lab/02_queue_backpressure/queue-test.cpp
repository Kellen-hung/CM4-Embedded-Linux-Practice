#include <gst/gst.h>
// #include <gst/app/gstappsink.h>
// #include <opencv2/opencv.hpp>
#include <iostream>
#include <cstdlib>
#include <chrono>
#include <thread>

static auto last_time = std::chrono::steady_clock::now();
static int frame_count = 0;

static void on_handoff(GstElement *identity, GstBuffer *buffer, gpointer user_data)
{
    auto now = std::chrono::steady_clock::now();

    double interval = std::chrono::duration<double>(now - last_time).count();

    frame_count++;

    std::cout << "Frame " << frame_count << " | interval = " << interval << " s" << std::endl;
    last_time = now;
}

int main(int argc, char *argv[])
{
    /* Get frame with Gstreamer!! */
    // init 
    gst_init(&argc, &argv);

    // const char *rtsp_url = std::getenv("RTSP_URL");

    // if (rtsp_url == nullptr) {
    //     std::cerr << "RTSP_URL is not set" << std::endl;
    //     return 1;
    // }

    // // create pipeline
    // std::string pipeline_str =
    //     "rtspsrc location=\"" + std::string(rtsp_url) + "\" latency=200 protocols=tcp ! "
    //     "application/x-rtp,media=video,encoding-name=H264 ! "
    //     "rtph264depay wait-for-keyframe=true request-keyframe=true ! "
    //     "video/x-h264,alignment=au ! "
    //     "h264parse ! "
    //     "avdec_h264 ! "
    //     "videoconvert ! "
    //     "video/x-raw,format=BGR ! "
    //     "appsink name=sink max-buffers=1 drop=true";

    // std::string pipeline_str =
    //     "videotestsrc is-live=true ! "
    //     "video/x-raw,width=320,height=240,framerate=15/1 ! "
    //     "identity name=observer signal-handoffs=true ! "
    //     "identity sleep-time=500000 ! "
    //     "fakesink sync=false";

    std::string pipeline_str =
        "videotestsrc is-live=true ! "
        "video/x-raw,width=320,height=240,framerate=15/1 ! "
        "identity name=observer signal-handoffs=true ! "
        "queue max-size-buffers=5 max-size-bytes=0 max-size-time=0 leaky=downstream ! "
        "identity sleep-time=500000 ! "
        "fakesink sync=false";

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

    GstElement *observer = gst_bin_get_by_name(GST_BIN(pipeline), "observer");

    if (observer == nullptr) {
        std::cerr << "Failed to get observer" << std::endl;

        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);

        return 1;
    }

    g_signal_connect(
        observer,
        "handoff",
        G_CALLBACK(on_handoff),
        nullptr
    );

    std::this_thread::sleep_for(
        std::chrono::seconds(10)
    );

    gst_element_set_state(pipeline, GST_STATE_NULL);

    gst_object_unref(observer);
    gst_object_unref(pipeline);

    return 0;

    // // get sample from appsink
    // GstElement *sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");

    // if (sink == nullptr) {
    //     std::cerr << "Failed to get appsink" << std::endl;

    //     gst_element_set_state(pipeline, GST_STATE_NULL);
    //     gst_object_unref(pipeline);

    //     return 1;
    // }

    // std::cout << "Waiting for sample..." << std::endl;

    // int frame_count = 0;

    // GstClockTime first_pts = GST_CLOCK_TIME_NONE;
    // auto start_time = std::chrono::steady_clock::now();

    // while (true) {
    //     GstSample *sample = gst_app_sink_try_pull_sample(
    //         GST_APP_SINK(sink),
    //         2 * GST_SECOND
    //     );

    //     if (sample == nullptr) {
    //         std::cerr << "Failed to get sample" << std::endl;
    //         break;
    //     }

    //     GstBuffer *buffer = gst_sample_get_buffer(sample);

    //     if (buffer == nullptr) {
    //         std::cerr << "Failed to get buffer" << std::endl;
    //         gst_sample_unref(sample);
    //         break;
    //     }

    //     GstClockTime pts = GST_BUFFER_PTS(buffer);

    //     if (first_pts == GST_CLOCK_TIME_NONE) {
    //         first_pts = pts;
    //     }

    //     auto now = std::chrono::steady_clock::now();

    //     double wall_time = std::chrono::duration<double>(now - start_time).count();

    //     double stream_time = (pts - first_pts) / static_cast<double>(GST_SECOND);

    //     frame_count++;

    //     std::cout << "Frame " << frame_count << " | stream=" << stream_time << " s" << " | wall=" << wall_time << " s" << " | lag=" << wall_time - stream_time << " s" << std::endl;

    //     gst_sample_unref(sample);

    //     std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // }

    // end
    // gst_object_unref(sink);
    // gst_element_set_state(pipeline, GST_STATE_NULL);
    // gst_object_unref(pipeline);

    // return 0;
}