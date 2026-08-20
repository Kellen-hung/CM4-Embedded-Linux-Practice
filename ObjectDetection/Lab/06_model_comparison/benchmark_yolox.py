import cv2
import numpy as np
import time

MODEL_SIZE = 416
WARMUP = 5
RUNS = 30

net = cv2.dnn.readNetFromONNX("models/yolox-nano/yolox_nano.onnx")

image = cv2.imread("right_dewarp.jpg")

if image is None:
    raise RuntimeError("Failed to load image")

h, w = image.shape[:2]

scale = min(MODEL_SIZE / w, MODEL_SIZE / h)
new_w = int(w * scale)
new_h = int(h * scale)

resized = cv2.resize(image, (new_w, new_h))

letterbox = np.full((MODEL_SIZE, MODEL_SIZE, 3), 114, dtype=np.uint8)
letterbox[:new_h, :new_w] = resized

blob = cv2.dnn.blobFromImage(letterbox, scalefactor=1.0, size=(MODEL_SIZE, MODEL_SIZE), swapRB=False, crop=False)

print("Warmup...")

for _ in range(WARMUP):
    net.setInput(blob)
    net.forward()

print("Benchmark...")

times = []

for i in range(RUNS):
    start = time.perf_counter()

    net.setInput(blob)
    net.forward()

    end = time.perf_counter()

    ms = (end - start) * 1000
    times.append(ms)

    print(f"{i + 1:2d}: {ms:.2f} ms")

avg = sum(times) / len(times)

print()
print(f"Average : {avg:.2f} ms")
print(f"Min     : {min(times):.2f} ms")
print(f"Max     : {max(times):.2f} ms")
print(f"FPS     : {1000.0 / avg:.2f}")