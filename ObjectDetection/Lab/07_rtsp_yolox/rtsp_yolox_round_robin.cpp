#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>

#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>

#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

constexpr int VIEW_W = 416;
constexpr int VIEW_H = 312;
constexpr int MODEL_SIZE = 416;

constexpr double H_FOV = 100.0;
constexpr double V_FOV = 83.56;
constexpr double PI = 3.14159265358979323846;

constexpr float SCORE_THRESHOLD = 0.4f;
constexpr float NMS_THRESHOLD = 0.45f;

const std::string MODEL_PATH = "../06_model_comparison/models/yolox-nano/yolox_nano.onnx";

volatile sig_atomic_t running = 1;

struct Detection {
    int class_id;
    float score;
    cv::Rect box;
};

struct GlobalDetection {
    int class_id;
    float score;
    double yaw;
    double pitch;
    int view_id;
};

const std::vector<std::string> COCO_CLASSES = {
    "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat", "traffic light",
    "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow",
    "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
    "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove", "skateboard", "surfboard",
    "tennis racket", "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple",
    "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch",
    "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse", "remote", "keyboard", "cell phone",
    "microwave", "oven", "toaster", "sink", "refrigerator", "book", "clock", "vase", "scissors", "teddy bear",
    "hair drier", "toothbrush"
};

void signalHandler(int)
{
    running = 0;
}

void buildMap(int src_w, int src_h, double yaw_deg, cv::Mat &map_x, cv::Mat &map_y)
{
    map_x.create(VIEW_H, VIEW_W, CV_32FC1);
    map_y.create(VIEW_H, VIEW_W, CV_32FC1);

    double yaw = yaw_deg * PI / 180.0;
    double h_fov = H_FOV * PI / 180.0;
    double v_fov = V_FOV * PI / 180.0;

    double tan_h = std::tan(h_fov / 2.0);
    double tan_v = std::tan(v_fov / 2.0);

    double cos_yaw = std::cos(yaw);
    double sin_yaw = std::sin(yaw);

    for (int y = 0; y < VIEW_H; y++) {
        for (int x = 0; x < VIEW_W; x++) {
            double nx = (2.0 * (x + 0.5) / VIEW_W - 1.0) * tan_h;
            double ny = (2.0 * (y + 0.5) / VIEW_H - 1.0) * tan_v;

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

void buildYoloGrid(std::vector<float> &grid_x, std::vector<float> &grid_y, std::vector<float> &strides)
{
    for (int stride : {8, 16, 32}) {
        int size = MODEL_SIZE / stride;

        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                grid_x.push_back(static_cast<float>(x));
                grid_y.push_back(static_cast<float>(y));
                strides.push_back(static_cast<float>(stride));
            }
        }
    }
}

double normalizeYaw(double yaw)
{
    while (yaw >= 180.0)
        yaw -= 360.0;

    while (yaw < -180.0)
        yaw += 360.0;

    return yaw;
}

double boxCenterToGlobalYaw(const cv::Rect &box, double view_yaw)
{
    double center_x = box.x + box.width / 2.0;

    double normalized_x = 2.0 * center_x / VIEW_W - 1.0;

    double half_fov = H_FOV * PI / 180.0;

    double local_yaw = std::atan(normalized_x * std::tan(half_fov / 2.0)) * 180.0 / PI;

    return normalizeYaw(view_yaw + local_yaw);
}

double boxCenterToPitch(const cv::Rect &box)
{
    double center_x = box.x + box.width / 2.0;
    double center_y = box.y + box.height / 2.0;

    double nx = (2.0 * center_x / VIEW_W - 1.0) * std::tan(H_FOV * PI / 360.0);
    double ny = (2.0 * center_y / VIEW_H - 1.0) * std::tan(V_FOV * PI / 360.0);

    double length = std::sqrt(nx * nx + ny * ny + 1.0);

    return -std::asin(ny / length) * 180.0 / PI;
}

std::vector<Detection> postprocess(const cv::Mat &output, const std::vector<float> &grid_x, const std::vector<float> &grid_y, const std::vector<float> &strides)
{
    cv::Mat predictions = output.reshape(1, 3549);

    std::vector<cv::Rect> boxes;
    std::vector<float> scores;
    std::vector<int> class_ids;

    for (int i = 0; i < predictions.rows; i++) {
        const float *data = predictions.ptr<float>(i);

        float objectness = data[4];

        int best_class = 0;
        float best_class_score = data[5];

        for (int c = 1; c < 80; c++) {
            if (data[5 + c] > best_class_score) {
                best_class_score = data[5 + c];
                best_class = c;
            }
        }

        float score = objectness * best_class_score;

        if (score < SCORE_THRESHOLD)
            continue;

        float cx = (data[0] + grid_x[i]) * strides[i];
        float cy = (data[1] + grid_y[i]) * strides[i];
        float bw = std::exp(data[2]) * strides[i];
        float bh = std::exp(data[3]) * strides[i];

        int x = static_cast<int>(cx - bw / 2.0f);
        int y = static_cast<int>(cy - bh / 2.0f);
        int w = static_cast<int>(bw);
        int h = static_cast<int>(bh);

        boxes.emplace_back(x, y, w, h);
        scores.push_back(score);
        class_ids.push_back(best_class);
    }

    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, scores, SCORE_THRESHOLD, NMS_THRESHOLD, indices);

    std::vector<Detection> detections;

    for (int index : indices) {
        cv::Rect box = boxes[index];

        box &= cv::Rect(0, 0, VIEW_W, VIEW_H);

        if (box.width <= 0 || box.height <= 0)
            continue;

        detections.push_back({
            class_ids[index],
            scores[index],
            box
        });
    }

    return detections;
}

void drawDetections(cv::Mat &image, const std::vector<Detection> &detections)
{
    for (const Detection &d : detections) {
        cv::rectangle(image, d.box, cv::Scalar(0, 255, 0), 2);

        std::string label = COCO_CLASSES[d.class_id] + " " + cv::format("%.2f", d.score);

        int baseline = 0;
        cv::Size text_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);

        int text_y = std::max(d.box.y - 5, text_size.height);

        cv::putText(image, label, cv::Point(d.box.x, text_y),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
    }
}

double yawDistance(double a, double b)
{
    double diff = std::fabs(a - b);

    if (diff > 180.0)
        diff = 360.0 - diff;

    return diff;
}

double pitchDistance(double a, double b)
{
    return std::fabs(a - b);
}

std::vector<GlobalDetection> mergeDetections(const std::vector<GlobalDetection> &input)
{
    constexpr double YAW_THRESHOLD = 5.0;
    constexpr double PITCH_THRESHOLD = 5.0;

    std::vector<GlobalDetection> merged;

    for (const GlobalDetection &d : input) {
        int duplicate_index = -1;

        for (int i = 0; i < static_cast<int>(merged.size()); i++) {
            if (merged[i].class_id != d.class_id)
                continue;

            if (yawDistance(merged[i].yaw, d.yaw) >= YAW_THRESHOLD)
                continue;

            if (pitchDistance(merged[i].pitch, d.pitch) >= PITCH_THRESHOLD)
                continue;

            duplicate_index = i;
            break;
        }

        if (duplicate_index == -1) {
            merged.push_back(d);
        }
        else if (d.score > merged[duplicate_index].score) {
            merged[duplicate_index] = d;
        }
    }

    return merged;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <RTSP_URL> <THREADS>\n";
        return 1;
    }

    int threads = std::stoi(argv[2]);

    cv::setNumThreads(threads);

    std::cout << "OpenCV threads: " << cv::getNumThreads() << "\n";

    struct sigaction sa {};
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);

    std::cout << "Loading YOLOX-Nano...\n";

    cv::dnn::Net net = cv::dnn::readNetFromONNX(MODEL_PATH);

    if (net.empty()) {
        std::cerr << "Failed to load model: " << MODEL_PATH << "\n";
        return 1;
    }

    std::vector<float> grid_x;
    std::vector<float> grid_y;
    std::vector<float> strides;

    buildYoloGrid(grid_x, grid_y, strides);

    std::cout << "YOLOX ready\n";

    gst_init(&argc, &argv);

    std::string pipeline_desc =
        "rtspsrc location=" + std::string(argv[1]) + " latency=0 drop-on-latency=true protocols=tcp ! "
        "application/x-rtp,media=video,encoding-name=H264 ! "
        "rtph264depay wait-for-keyframe=true request-keyframe=true ! "
        "video/x-h264,alignment=au ! "
        "h264parse ! "
        "avdec_h264 ! "
        "queue max-size-buffers=1 leaky=downstream ! "
        "videorate drop-only=true ! "
        "video/x-raw,framerate=5/1 ! "
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
    std::vector<cv::Mat> map_xs(4);
    std::vector<cv::Mat> map_ys(4);

    bool maps_ready = false;
    int current_view = 0;
    uint64_t processed = 0;
    bool saved_result[4] = {false, false, false, false};
    std::vector<GlobalDetection> round_detections;

    cv::Mat dewarped;
    cv::Mat canvas(MODEL_SIZE, MODEL_SIZE, CV_8UC3);

    while (running) {
        GstSample *sample = gst_app_sink_try_pull_sample(sink, 200 * GST_MSECOND);

        if (!sample)
            continue;

        GstCaps *caps = gst_sample_get_caps(sample);
        GstBuffer *buffer = gst_sample_get_buffer(sample);

        GstVideoInfo info;

        if (!gst_video_info_from_caps(&info, caps)) {
            gst_sample_unref(sample);
            continue;
        }

        int width = GST_VIDEO_INFO_WIDTH(&info);
        int height = GST_VIDEO_INFO_HEIGHT(&info);
        int stride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);

        if (!maps_ready) {
            std::cout << "RTSP frame: " << width << "x" << height << "\n";
            std::cout << "Building projection maps...\n";

            for (int i = 0; i < 4; i++)
                buildMap(width, height, yaws[i], map_xs[i], map_ys[i]);

            maps_ready = true;

            std::cout << "Maps ready\n\n";
        }

        GstMapInfo gst_map;

        if (!gst_buffer_map(buffer, &gst_map, GST_MAP_READ)) {
            gst_sample_unref(sample);
            continue;
        }

        cv::Mat frame(height, width, CV_8UC3, gst_map.data, stride);

        auto total_start = std::chrono::steady_clock::now();

        auto remap_start = std::chrono::steady_clock::now();

        cv::remap(frame, dewarped, map_xs[current_view], map_ys[current_view], cv::INTER_LINEAR, cv::BORDER_WRAP);

        auto remap_end = std::chrono::steady_clock::now();

        auto preprocess_start = std::chrono::steady_clock::now();

        canvas.setTo(cv::Scalar(114, 114, 114));
        dewarped.copyTo(canvas(cv::Rect(0, 0, VIEW_W, VIEW_H)));

        cv::Mat blob;
        cv::dnn::blobFromImage(canvas, blob, 1.0, cv::Size(MODEL_SIZE, MODEL_SIZE), cv::Scalar(), false, false);

        auto preprocess_end = std::chrono::steady_clock::now();

        auto inference_start = std::chrono::steady_clock::now();

        net.setInput(blob);
        cv::Mat output = net.forward();

        auto inference_end = std::chrono::steady_clock::now();

        auto post_start = std::chrono::steady_clock::now();

        std::vector<Detection> detections = postprocess(output, grid_x, grid_y, strides);
        drawDetections(dewarped, detections);

        auto post_end = std::chrono::steady_clock::now();
        auto total_end = post_end;

        double remap_ms = std::chrono::duration<double, std::milli>(remap_end - remap_start).count();
        double preprocess_ms = std::chrono::duration<double, std::milli>(preprocess_end - preprocess_start).count();
        double inference_ms = std::chrono::duration<double, std::milli>(inference_end - inference_start).count();
        double post_ms = std::chrono::duration<double, std::milli>(post_end - post_start).count();
        double total_ms = std::chrono::duration<double, std::milli>(total_end - total_start).count();

        std::cout << "view=" << current_view
                  << " yaw=" << yaws[current_view]
                  << " remap=" << remap_ms
                  << " pre=" << preprocess_ms
                  << " infer=" << inference_ms
                  << " post=" << post_ms
                  << " total=" << total_ms
                  << " ms";

        for (const Detection &d : detections) {
            double object_yaw = boxCenterToGlobalYaw(d.box, yaws[current_view]);
            double object_pitch = boxCenterToPitch(d.box);

            std::cout << "  [" << COCO_CLASSES[d.class_id]
                    << " " << d.score
                    << " yaw=" << object_yaw
                    << "° pitch=" << object_pitch
                    << "°]";

            round_detections.push_back({
                d.class_id,
                d.score,
                object_yaw,
                object_pitch,
                current_view
            });
        }

        std::cout << "\n";

        if (!saved_result[current_view]) {
            std::string filename = "./images/result_yaw_" + std::to_string(static_cast<int>(yaws[current_view])) + ".jpg";

            cv::imwrite(filename, dewarped);

            std::cout << "Saved " << filename << "\n";

            saved_result[current_view] = true;
        }

        if (current_view == 3) {
            std::vector<GlobalDetection> merged = mergeDetections(round_detections);

            std::cout << "\n===== 360 MERGED RESULT =====\n";

            for (const GlobalDetection &d : merged) {
                std::cout << COCO_CLASSES[d.class_id]
                        << " score=" << d.score
                        << " yaw=" << d.yaw << "°"
                        << " pitch=" << d.pitch << "°"
                        << " source_view=" << d.view_id
                        << "\n";
            }

            std::cout << "Raw detections: " << round_detections.size()
                    << " -> Merged: " << merged.size()
                    << "\n=============================\n";

            round_detections.clear();
        }

        current_view = (current_view + 1) % 4;
        processed++;

        gst_buffer_unmap(buffer, &gst_map);
        gst_sample_unref(sample);
    }

    std::cout << "\nStopping...\n";

    gst_element_set_state(pipeline, GST_STATE_NULL);

    gst_object_unref(sink_element);
    gst_object_unref(pipeline);

    return 0;
}