# Lab 07 - RTSP + Dewarp + YOLOX

## Goal

Build a real-time object detection pipeline for a 360° RTSP stream on Raspberry Pi CM4.

## Pipeline

RTSP H.264 Stream  
→ GStreamer Decode  
→ 4-View Dewarp  
→ YOLOX-Nano  
→ Detection Post-processing

The 360° image is divided into four perspective views:

- 0°
- 90°
- 180°
- -90°

Each view uses:

- Resolution: 416×312
- Horizontal FOV: 100°
- Vertical FOV: 83.56°

The four views are processed in round-robin order.

## Model

YOLOX-Nano ONNX is executed with OpenCV DNN.

OpenCV 5.0 was built manually with ARM optimizations including:

- NEON
- MLAS
- KleidiCV
- `-O3`

Standalone YOLOX inference is about 117 ms.

## Post-processing

Implemented:

- YOLOX bounding box decoding
- Score threshold
- NMS
- Bounding box visualization
- Global yaw / pitch conversion
- Duplicate detection merging between overlapping views

## Performance Findings

- OpenCV 4.10 C++ inference: ~210 ms
- OpenCV 5.0 C++ inference: ~117 ms
- NCNN CPU inference: ~500 ms
- RTSP + YOLOX causes CPU contention
- Reducing full-resolution BGR conversion from 15 FPS to 5 FPS reduces unnecessary CPU workload

## Conclusion

Multi-view dewarping provides much better detection results than directly running YOLOX on the full equirectangular 360° image.

The main remaining bottleneck is CPU contention between H.264 decoding and YOLOX inference.

Next experiment: GPU inference with Vulkan.