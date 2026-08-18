import os
import gi
import cv2
import numpy as np

gi.require_version("Gst", "1.0")
from gi.repository import Gst


Gst.init(None)

rtsp_url = os.environ["RTSP_URL"]

pipeline_str = f"""
rtspsrc location="{rtsp_url}" latency=200 protocols=tcp !
application/x-rtp,media=video,encoding-name=H264 !
rtph264depay !
h264parse !
avdec_h264 !
videoconvert !
video/x-raw,format=BGR !
appsink name=sink
"""

pipeline = Gst.parse_launch(pipeline_str)
sink = pipeline.get_by_name("sink")

ret = pipeline.set_state(Gst.State.PLAYING)

if ret == Gst.StateChangeReturn.FAILURE:
    print("Failed to start pipeline")
    pipeline.set_state(Gst.State.NULL)
    raise SystemExit(1)

print("Waiting for frame...")

# sample = sink.emit("try-pull-sample", 10 * Gst.SECOND)

# if sample is None:
#     print("Failed to get frame")
#     pipeline.set_state(Gst.State.NULL)
#     raise SystemExit(1)

sample = None

for i in range(30):
    sample = sink.emit("try-pull-sample", 10 * Gst.SECOND)

    if sample is None:
        print("Failed to get frame")
        pipeline.set_state(Gst.State.NULL)
        raise SystemExit(1)

    print(f"Frame {i + 1}/30")

buffer = sample.get_buffer()
caps = sample.get_caps()

structure = caps.get_structure(0)

if hasattr(structure, "_StructureWrapper__structure"):
    structure = structure._StructureWrapper__structure

_, width = structure.get_int("width")
_, height = structure.get_int("height")

print("Caps:", caps.to_string())
print("Width:", width)
print("Height:", height)
print("Buffer size:", buffer.get_size())
print("Expected BGR size:", width * height * 3)

success, map_info = buffer.map(Gst.MapFlags.READ)

if not success:
    print("Failed to map buffer")
    pipeline.set_state(Gst.State.NULL)
    raise SystemExit(1)

frame = np.ndarray(
    shape=(height, width, 3),
    dtype=np.uint8,
    buffer=map_info.data
).copy()

buffer.unmap(map_info)

print(type(frame))
print(frame.shape)

cv2.imwrite("output/gstreamer_capture.jpg", frame)

print("Saved: output/gstreamer_capture.jpg")

pipeline.set_state(Gst.State.NULL)