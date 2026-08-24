import cv2
import numpy as np
import time

MODEL_SIZE = 416
SCORE_THRESHOLD = 0.3
NMS_THRESHOLD = 0.45

WARMUP = 5
RUNS = 30

net = cv2.dnn.readNetFromONNX("models/yolox-nano/yolox_nano.onnx")

image = cv2.imread("right_dewarp.jpg")

if image is None:
    raise RuntimeError("Failed to load image")


def preprocess(image):
    h, w = image.shape[:2]

    scale = min(MODEL_SIZE / w, MODEL_SIZE / h)
    new_w = int(w * scale)
    new_h = int(h * scale)

    resized = cv2.resize(image, (new_w, new_h))

    letterbox = np.full((MODEL_SIZE, MODEL_SIZE, 3), 114, dtype=np.uint8)
    letterbox[:new_h, :new_w] = resized

    blob = cv2.dnn.blobFromImage(letterbox, scalefactor=1.0, size=(MODEL_SIZE, MODEL_SIZE), swapRB=False, crop=False)

    return blob, scale


def demo_postprocess(output):
    grids = []
    expanded_strides = []

    for stride in [8, 16, 32]:
        hsize = MODEL_SIZE // stride
        wsize = MODEL_SIZE // stride

        xv, yv = np.meshgrid(np.arange(wsize), np.arange(hsize))
        grid = np.stack((xv, yv), axis=2).reshape(1, -1, 2)

        grids.append(grid)
        expanded_strides.append(np.full((1, grid.shape[1], 1), stride))

    grids = np.concatenate(grids, axis=1)
    expanded_strides = np.concatenate(expanded_strides, axis=1)

    output[..., :2] = (output[..., :2] + grids) * expanded_strides
    output[..., 2:4] = np.exp(output[..., 2:4]) * expanded_strides

    return output


def postprocess(output, scale):
    predictions = demo_postprocess(output.copy())[0]

    boxes = predictions[:, :4]
    objectness = predictions[:, 4]
    class_scores = predictions[:, 5:]

    class_ids = np.argmax(class_scores, axis=1)
    scores = objectness * class_scores[np.arange(len(class_ids)), class_ids]

    mask = scores >= SCORE_THRESHOLD

    boxes = boxes[mask]
    scores = scores[mask]
    class_ids = class_ids[mask]

    boxes_xyxy = np.zeros_like(boxes)

    boxes_xyxy[:, 0] = boxes[:, 0] - boxes[:, 2] / 2
    boxes_xyxy[:, 1] = boxes[:, 1] - boxes[:, 3] / 2
    boxes_xyxy[:, 2] = boxes[:, 0] + boxes[:, 2] / 2
    boxes_xyxy[:, 3] = boxes[:, 1] + boxes[:, 3] / 2

    boxes_xyxy /= scale

    nms_boxes = []

    for x1, y1, x2, y2 in boxes_xyxy:
        nms_boxes.append([int(x1), int(y1), int(x2 - x1), int(y2 - y1)])

    indices = cv2.dnn.NMSBoxes(nms_boxes, scores.tolist(), SCORE_THRESHOLD, NMS_THRESHOLD)

    return len(indices)


print("Warmup...")

for _ in range(WARMUP):
    blob, scale = preprocess(image)
    net.setInput(blob)
    output = net.forward()
    postprocess(output, scale)


preprocess_times = []
inference_times = []
postprocess_times = []
total_times = []

print("Benchmark...")

for i in range(RUNS):
    total_start = time.perf_counter()

    start = time.perf_counter()
    blob, scale = preprocess(image)
    end = time.perf_counter()
    preprocess_ms = (end - start) * 1000

    start = time.perf_counter()
    net.setInput(blob)
    output = net.forward()
    end = time.perf_counter()
    inference_ms = (end - start) * 1000

    start = time.perf_counter()
    detections = postprocess(output, scale)
    end = time.perf_counter()
    postprocess_ms = (end - start) * 1000

    total_ms = (time.perf_counter() - total_start) * 1000

    preprocess_times.append(preprocess_ms)
    inference_times.append(inference_ms)
    postprocess_times.append(postprocess_ms)
    total_times.append(total_ms)

    print(f"{i + 1:2d}: pre={preprocess_ms:6.2f} ms  infer={inference_ms:6.2f} ms  post={postprocess_ms:6.2f} ms  total={total_ms:6.2f} ms  detections={detections}")


def show_result(name, values):
    print(f"{name:12s}: avg={np.mean(values):6.2f} ms  min={np.min(values):6.2f}  max={np.max(values):6.2f}")


print()
show_result("Preprocess", preprocess_times)
show_result("Inference", inference_times)
show_result("Postprocess", postprocess_times)
show_result("Total", total_times)

avg_total = np.mean(total_times)

print()
print(f"E2E FPS     : {1000.0 / avg_total:.2f}")