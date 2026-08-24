# Lab 08 - GPU Inference

## Goal

Test YOLOX-Nano inference on Raspberry Pi CM4 using NCNN Vulkan with VideoCore VI.

## Result

Vulkan and NCNN successfully detected the V3D GPU.

- NCNN CPU (4 threads): 558.06 ms
- NCNN Vulkan / V3D: 7591.92 ms

## Conclusion

VideoCore VI can run YOLOX-Nano through Vulkan, but inference is much slower than CPU.

CPU inference is more suitable for this workload on CM4.