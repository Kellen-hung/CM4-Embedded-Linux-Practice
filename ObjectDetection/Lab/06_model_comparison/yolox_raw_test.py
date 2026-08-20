import cv2
import numpy as np

MODEL_SIZE = 416

DIRECTION = "center"

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

def demo_postprocess(outputs):
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

    outputs[..., :2] = (outputs[..., :2] + grids) * expanded_strides
    outputs[..., 2:4] = np.exp(outputs[..., 2:4]) * expanded_strides

    return outputs


def nms(boxes, scores, score_threshold=0.3, nms_threshold=0.45):
    indices = cv2.dnn.NMSBoxes(boxes, scores, score_threshold, nms_threshold)

    if len(indices) == 0:
        return []

    return indices.flatten()

net = cv2.dnn.readNetFromONNX("models/yolox-nano/yolox_nano.onnx")

image = cv2.imread(f"{DIRECTION}_dewarp.jpg")

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

net.setInput(blob)
output = net.forward()

predictions = demo_postprocess(output.copy())[0]

boxes = predictions[:, :4]
objectness = predictions[:, 4]
class_scores = predictions[:, 5:]

class_ids = np.argmax(class_scores, axis=1)
scores = objectness * class_scores[np.arange(len(class_ids)), class_ids]

mask = scores >= 0.3

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

keep = nms(nms_boxes, scores.tolist())

print("Detections:", len(keep))

for i in keep:
    class_id = int(class_ids[i])
    score = float(scores[i])

    x1, y1, x2, y2 = boxes_xyxy[i]

    x1 = int(max(0, min(x1, w - 1)))
    y1 = int(max(0, min(y1, h - 1)))
    x2 = int(max(0, min(x2, w - 1)))
    y2 = int(max(0, min(y2, h - 1)))

    label = f"{COCO_CLASSES[class_id]} {score:.2f}"

    print(f"{COCO_CLASSES[class_id]:15s} score={score:.3f} box=({x1}, {y1})-({x2}, {y2})")

    cv2.rectangle(image, (x1, y1), (x2, y2), (0, 255, 0), 2)
    cv2.putText(image, label, (x1, max(y1 - 8, 20)), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)

output_path = f"results-img/{DIRECTION}_yolox.jpg"
cv2.imwrite(output_path, image)

print("Saved:", output_path)

# print("Original:", w, "x", h)
# print("Resized:", new_w, "x", new_h)
# print("Blob shape:", blob.shape)
# print("Output shape:", output.shape)
# print("Output min:", output.min())
# print("Output max:", output.max())

