#include "rtsp_capture.hpp"

#include "config.hpp"

#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include <stdexcept>
#include <utility>

struct RtspFrame::Impl {
    GstSample* sample = nullptr;
    GstBuffer* buffer = nullptr;
    GstMapInfo map_info{};
    bool mapped = false;
    cv::Mat image;

    ~Impl()
    {
        if (mapped)
            gst_buffer_unmap(buffer, &map_info);
        if (sample)
            gst_sample_unref(sample);
    }
};

struct RtspCapture::Impl {
    GstElement* pipeline = nullptr;
    GstElement* sink_element = nullptr;
    GstAppSink* sink = nullptr;

    ~Impl()
    {
        if (pipeline)
            gst_element_set_state(pipeline, GST_STATE_NULL);
        if (sink_element)
            gst_object_unref(sink_element);
        if (pipeline)
            gst_object_unref(pipeline);
    }
};

RtspFrame::RtspFrame() = default;
RtspFrame::RtspFrame(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
RtspFrame::~RtspFrame() = default;
RtspFrame::RtspFrame(RtspFrame&&) noexcept = default;
RtspFrame& RtspFrame::operator=(RtspFrame&&) noexcept = default;

const cv::Mat& RtspFrame::image() const
{
    if (!impl_)
        throw std::runtime_error("No RTSP frame is available");
    return impl_->image;
}

RtspFrame::operator bool() const
{
    return impl_ != nullptr;
}

RtspCapture::RtspCapture(const std::string& url, Decoder decoder) : impl_(std::make_unique<Impl>())
{
    gst_init(nullptr, nullptr);
    gchar* escaped_url = g_strescape(url.c_str(), nullptr);
    const std::string decoder_element = decoder == Decoder::Hardware ? "v4l2h264dec" : "avdec_h264";
    const std::string pipeline_description =
        "rtspsrc location=\"" + std::string(escaped_url) +
        "\" latency=0 drop-on-latency=true protocols=tcp ! "
        "application/x-rtp,media=video,encoding-name=H264 ! "
        "rtph264depay wait-for-keyframe=true request-keyframe=true ! "
        "h264parse ! video/x-h264,stream-format=byte-stream,alignment=au ! " +
        decoder_element + " ! "
        "queue max-size-buffers=1 leaky=downstream ! "
        "videorate drop-only=true ! video/x-raw,framerate=" +
        std::to_string(config::DETECTION_FPS) + "/1 ! "
        "videoconvert ! video/x-raw,format=BGR ! "
        "appsink name=sink max-buffers=1 drop=true sync=false";
    g_free(escaped_url);

    GError* error = nullptr;
    impl_->pipeline = gst_parse_launch(pipeline_description.c_str(), &error);
    if (!impl_->pipeline) {
        const std::string message = error ? error->message : "unknown error";
        if (error)
            g_error_free(error);
        throw std::runtime_error("Failed to create GStreamer pipeline: " + message);
    }

    impl_->sink_element = gst_bin_get_by_name(GST_BIN(impl_->pipeline), "sink");
    if (!impl_->sink_element)
        throw std::runtime_error("GStreamer pipeline has no appsink named sink");
    impl_->sink = GST_APP_SINK(impl_->sink_element);

    const GstStateChangeReturn state = gst_element_set_state(impl_->pipeline, GST_STATE_PLAYING);
    if (state == GST_STATE_CHANGE_FAILURE)
        throw std::runtime_error("Failed to start GStreamer pipeline");
}

RtspCapture::~RtspCapture() = default;

RtspFrame RtspCapture::pullFrame(unsigned int timeout_ms)
{
    GstSample* sample = gst_app_sink_try_pull_sample(
        impl_->sink, static_cast<GstClockTime>(timeout_ms) * GST_MSECOND);
    if (!sample)
        return {};

    auto frame = std::make_unique<RtspFrame::Impl>();
    frame->sample = sample;
    frame->buffer = gst_sample_get_buffer(sample);
    GstCaps* caps = gst_sample_get_caps(sample);
    GstVideoInfo video_info{};
    if (!caps || !frame->buffer || !gst_video_info_from_caps(&video_info, caps))
        return {};
    if (GST_VIDEO_INFO_FORMAT(&video_info) != GST_VIDEO_FORMAT_BGR)
        return {};
    if (!gst_buffer_map(frame->buffer, &frame->map_info, GST_MAP_READ))
        return {};
    frame->mapped = true;

    const int width = GST_VIDEO_INFO_WIDTH(&video_info);
    const int height = GST_VIDEO_INFO_HEIGHT(&video_info);
    const int stride = GST_VIDEO_INFO_PLANE_STRIDE(&video_info, 0);
    const std::size_t offset = GST_VIDEO_INFO_PLANE_OFFSET(&video_info, 0);
    frame->image = cv::Mat(height, width, CV_8UC3, frame->map_info.data + offset, stride);
    return RtspFrame(std::move(frame));
}

bool RtspCapture::checkError(std::string& message)
{
    GstBus* bus = gst_element_get_bus(impl_->pipeline);
    GstMessage* event = gst_bus_pop_filtered(
        bus, static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
    gst_object_unref(bus);
    if (!event)
        return false;

    if (GST_MESSAGE_TYPE(event) == GST_MESSAGE_ERROR) {
        GError* error = nullptr;
        gchar* debug = nullptr;
        gst_message_parse_error(event, &error, &debug);
        message = error ? error->message : "unknown GStreamer error";
        if (debug && *debug)
            message += " (" + std::string(debug) + ")";
        g_clear_error(&error);
        g_free(debug);
    } else {
        message = "RTSP pipeline reached end of stream";
    }
    gst_message_unref(event);
    return true;
}
