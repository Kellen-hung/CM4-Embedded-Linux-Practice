import cv2
import ncnn
import numpy as np

MODEL_SIZE = 416
SCORE_THRESHOLD = 0.4
NMS_THRESHOLD = 0.5
REG_MAX = 7

DIRECTION = "center"

HEADS = [
    ("save_infer_model/scale_0.tmp_1", "save_infer_model/scale_4.tmp_1", 8),
    ("save_infer_model/scale_1.tmp_1", "save_infer_model/scale_5.tmp_1", 16),
    ("save_infer_model/scale_2.tmp_1", "save_infer_model/scale_6.tmp_1", 32),
    ("save_infer_model/scale_3.tmp_1", "save_infer_model/scale_7.tmp_1", 64),
]

COCO_CLASSES = [
    "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat", "traffic light",
    "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow",
    "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
    "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove", "skateboard", "surfboard",
    "tennis racket", "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple",
    "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch",
    "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse", "remote", "keyboard", "cell phone",
    "microwave", "oven", "toaster", "sink", "refrigerator", "book", "clock", "vase", "scissors", "teddy bear",
    "hair drier", "toothbrush"
]

def softmax(x):
    x = x - np.max(x)
    exp_x = np.exp(x)
    return exp_x / np.sum(exp_x)

def decode_head(cls_pred, box_pred, stride):
    detections = []

    feature_w = int(np.ceil(MODEL_SIZE / stride))

    for idx in range(cls_pred.shape[0]):
        class_id = int(np.argmax(cls_pred[idx]))
        score = float(cls_pred[idx, class_id])

        if score < SCORE_THRESHOLD:
            continue

        row = idx // feature_w
        col = idx % feature_w

        center_x = (col + 0.5) * stride
        center_y = (row + 0.5) * stride

        distances = []

        for side in range(4):
            logits = box_pred[idx, side * (REG_MAX + 1):(side + 1) * (REG_MAX + 1)]
            probs = softmax(logits)
            distance = np.sum(np.arange(REG_MAX + 1) * probs) * stride
            distances.append(distance)

        left, top, right, bottom = distances

        x1 = max(center_x - left, 0)
        y1 = max(center_y - top, 0)
        x2 = min(center_x + right, MODEL_SIZE)
        y2 = min(center_y + bottom, MODEL_SIZE)

        detections.append([x1, y1, x2, y2, score, class_id])

    return detections

def nms_per_class(detections):
    result = []

    for class_id in range(80):
        class_dets = [det for det in detections if det[5] == class_id]

        if not class_dets:
            continue

        boxes = []
        scores = []

        for x1, y1, x2, y2, score, _ in class_dets:
            boxes.append([x1, y1, x2 - x1, y2 - y1])
            scores.append(score)

        indices = cv2.dnn.NMSBoxes(boxes, scores, SCORE_THRESHOLD, NMS_THRESHOLD)

        for index in indices:
            result.append(class_dets[int(index)])

    return result


net = ncnn.Net()
net.opt.num_threads = 4

net.load_param("models/picodet_m_416.param")
net.load_model("models/picodet_m_416.bin")

image = cv2.imread(f"{DIRECTION}_dewarp.jpg")

if image is None:
    raise RuntimeError("Failed to load image")

h, w = image.shape[:2]

scale = min(MODEL_SIZE / w, MODEL_SIZE / h)
new_w = int(w * scale)
new_h = int(h * scale)

resized = cv2.resize(image, (new_w, new_h))

pad_left = (MODEL_SIZE - new_w) // 2
pad_top = (MODEL_SIZE - new_h) // 2

letterbox = np.zeros((MODEL_SIZE, MODEL_SIZE, 3), dtype=np.uint8)
letterbox[pad_top:pad_top + new_h, pad_left:pad_left + new_w] = resized

mat_in = ncnn.Mat.from_pixels(letterbox, ncnn.Mat.PixelType.PIXEL_BGR, MODEL_SIZE, MODEL_SIZE)

mean_vals = [103.53, 116.28, 123.675]
norm_vals = [0.017429, 0.017507, 0.017125]

mat_in.substract_mean_normalize(mean_vals, norm_vals)

ex = net.create_extractor()
ex.input("image", mat_in)

detections = []

for cls_name, box_name, stride in HEADS:
    _, cls_out = ex.extract(cls_name)
    _, box_out = ex.extract(box_name)

    cls_pred = np.array(cls_out)
    box_pred = np.array(box_out)

    detections.extend(decode_head(cls_pred, box_pred, stride))

detections = nms_per_class(detections)

print("Detections:", len(detections))

for x1, y1, x2, y2, score, class_id in sorted(detections, key=lambda x: x[4], reverse=True):
    # letterbox 416x416 -> original 1280x960
    x1 = (x1 - pad_left) / scale
    y1 = (y1 - pad_top) / scale
    x2 = (x2 - pad_left) / scale
    y2 = (y2 - pad_top) / scale

    x1 = max(0, min(x1, w - 1))
    y1 = max(0, min(y1, h - 1))
    x2 = max(0, min(x2, w - 1))
    y2 = max(0, min(y2, h - 1))

    print(f"{COCO_CLASSES[class_id]:15s} score={score:.3f} box=({x1:.0f}, {y1:.0f})-({x2:.0f}, {y2:.0f})")

    x1 = int(x1)
    y1 = int(y1)
    x2 = int(x2)
    y2 = int(y2)

    label = f"{COCO_CLASSES[class_id]} {score:.2f}"

    cv2.rectangle(image, (x1, y1), (x2, y2), (0, 255, 0), 2)
    cv2.putText(image, label, (x1, max(y1 - 8, 20)), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)

cv2.imwrite(f"{DIRECTION}_picodet.jpg", image)
print(f"Saved: {DIRECTION}_picodet.jpg")