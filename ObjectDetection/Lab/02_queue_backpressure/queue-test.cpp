#include <gst/gst.h>
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
}