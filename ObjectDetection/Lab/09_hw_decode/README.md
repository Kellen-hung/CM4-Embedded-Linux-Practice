# Lab 09 - H.264 Hardware Decode

## Goal

Test H.264 hardware decoding on Raspberry Pi CM4 and compare it with software decoding.

## Decoder

- Software: `avdec_h264`
- Hardware: `v4l2h264dec`

Hardware decoding uses the BCM2835 V4L2 video decoder.

## Resolution Limitation

The hardware decoder supports up to 1920×1920.

- 3840×1920 H.264 → Not supported
- 1920×960 H.264 → Supported

The test stream uses H.264, 1920×960, 15 FPS.

## Result

| Decoder | CPU Usage | YOLOX Inference |
| --- | ---: | ---: |
| `avdec_h264` | ~30–40% | ~148.4 ms |
| `v4l2h264dec` | ~5–10% | ~140.2 ms |

## Conclusion

Using `v4l2h264dec` successfully offloads H.264 decoding to the hardware video decoder.

The main benefit is significantly lower CPU usage, leaving more CPU resources available for YOLOX inference and other system tasks.