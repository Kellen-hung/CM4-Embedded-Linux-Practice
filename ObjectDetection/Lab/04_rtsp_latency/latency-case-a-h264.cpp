#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <iostream>
#include <cstdlib>
#include <csignal>
#include <cstdio>

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

    // std::string pipeline_str =
    //     "rtspsrc location=\"" + std::string(rtsp_url) + "\" latency=0 protocols=tcp ! "
    //     "application/x-rtp,media=video,encoding-name=H264 ! "
    //     "rtph264depay wait-for-keyframe=true request-keyframe=true ! "
    //     "video/x-h264,alignment=au ! "
    //     "h264parse ! "
    //     "avdec_h264 ! "
    //     "queue name=convert_q max-size-buffers=1 max-size-bytes=0 max-size-time=0 ! "
    //     "videoconvert ! "
    //     "video/x-raw,format=BGR ! "
    //     "appsink name=sink";

    std::string pipeline_str =
        "rtspsrc location=\"" + std::string(rtsp_url) + "\" latency=0 protocols=tcp ! "
        "application/x-rtp,media=video,encoding-name=H264 ! "
        "rtph264depay wait-for-keyframe=true request-keyframe=true ! "
        "video/x-h264,alignment=au ! "
        "h264parse ! "
        "video/x-h264,alignment=au ! "
        "appsink name=sink";

    GError *error = nullptr;
    GstElement *pipeline = gst_parse_launch(pipeline_str.c_str(), &error);

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

    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);

    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "Failed to start pipeline" << std::endl;
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        return 1;
    }

    std::cout << "Pipeline started" << std::endl;

    GstElement *sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");

    if (sink == nullptr) {
        std::cerr << "Failed to get appsink" << std::endl;
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        return 1;
    }

    std::cout << "Waiting for sample..." << std::endl;

    int print_count = 0;

    while (running) {
        GstSample *sample = gst_app_sink_try_pull_sample(GST_APP_SINK(sink), 2 * GST_SECOND);

        if (sample == nullptr) {
            if (!running) {
                break;
            }

            std::cerr << "Failed to get sample" << std::endl;
            continue;
        }

        GstBuffer *buffer = gst_sample_get_buffer(sample);
        const GstSegment *segment = gst_sample_get_segment(sample);

        if (buffer != nullptr && segment != nullptr) {
            GstClockTime pts = GST_BUFFER_PTS(buffer);

            if (GST_CLOCK_TIME_IS_VALID(pts)) {
                GstClockTime buffer_running_time = gst_segment_to_running_time(segment, GST_FORMAT_TIME, pts);
                GstClock *clock = gst_element_get_clock(pipeline);

                if (clock != nullptr && GST_CLOCK_TIME_IS_VALID(buffer_running_time)) {
                    GstClockTime now = gst_clock_get_time(clock);
                    GstClockTime base_time = gst_element_get_base_time(pipeline);
                    GstClockTime pipeline_running_time = now - base_time;

                    if (pipeline_running_time >= buffer_running_time) {
                        double age_ms = (pipeline_running_time - buffer_running_time) / 1000000.0;

                        if (print_count % 15 == 0) {
                            std::cout << "PTS: " << pts / 1000000.0
                                    << " ms | Pipeline: " << pipeline_running_time / 1000000.0
                                    << " ms | Age: " << age_ms << " ms" << std::endl;
                        }

                        print_count++;
                    } else {
                        std::cout << "Buffer running time is ahead of pipeline running time" << std::endl;
                    }

                    gst_object_unref(clock);
                }
            }

            // printed = true;
        }

        gst_sample_unref(sample);
    }

    std::cout << "\nStopping pipeline..." << std::endl;

    gst_object_unref(sink);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    std::cout << "Stopped cleanly" << std::endl;

    return 0;
}