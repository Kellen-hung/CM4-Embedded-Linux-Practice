# CM4 360° Object Detection

## Purpose

This is the integrated C++17 application produced from the experiments in `Lab/01` through `Lab/09`. It consumes a 360° H.264 RTSP stream, processes one perspective view per source frame, runs YOLOX-Nano, and merges detections after a four-view cycle.

## Architecture

`RTSP capture -> cached perspective dewarp -> YOLOX-Nano -> global yaw/pitch -> four-view merge`

The modules keep GStreamer capture, projection geometry, detector decode, and cross-view merging out of `main.cpp`. The `Lab/` tree remains the experimental record and is not needed at runtime.

## Requirements

- Raspberry Pi CM4 / Raspberry Pi 4 class system with `bcm2835-codec-decode`
- GStreamer 1.0 with app, video, RTSP, H.264, and `v4l2h264dec` plugins
- Self-built OpenCV 5 with DNN support
- CMake and a C++17 compiler
- YOLOX-Nano ONNX model at `models/yolox_nano.onnx` (or an override path)

## Build

```sh
cmake -S . -B build -DOpenCV_DIR=$HOME/opencv-5-install/lib/cmake/opencv5
cmake --build build -j2
```

## Run

```sh
./build/object_detection 'rtsp://IP/streaming'
./build/object_detection 'rtsp://IP/streaming' /path/to/yolox_nano.onnx
```

The URL is always supplied on the command line. Press Ctrl+C for a clean shutdown.

## Current Pipeline

The default input is H.264 1920x960 at 15 FPS. GStreamer uses zero-latency TCP RTSP, `v4l2h264dec`, a one-buffer downstream-leaky queue, and `videorate` to reduce BGR conversion and application processing to 5 FPS. The application caches four 416x312 maps at yaws 0°, 90°, 180°, and -90°, processes them round-robin, bottom-pads each view to 416x416 with value 114, and runs OpenCV 5 DNN with four OpenCV threads.

This design follows the measured results: hardware decode reduced decoder CPU use, early 5 FPS reduction limits pixel-conversion contention, OpenCV 5 CPU outperformed OpenCV 4 and NCNN/Vulkan, and perspective views detected objects better than resizing the full panorama.
