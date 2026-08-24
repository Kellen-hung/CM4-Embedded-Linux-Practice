import cv2
import numpy as np

MODEL_SIZE = 416
VIEW_W = 416
VIEW_H = 312

SCORE_THRESHOLD = 0.3
NMS_THRESHOLD = 0.45

MODEL = "../06_model_comparison/models/yolox-nano/yolox_nano.onnx"

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

net = cv2.dnn.readNetFromONNX(MODEL)


def prepare(image):
    if image.shape[1] != VIEW_W or image.shape[0] != VIEW_H:
        image = cv2.resize(image, (VIEW_W, VIEW_H))

    canvas = np.full((MODEL_SIZE, MODEL_SIZE, 3), 114, dtype=np.uint8)
    canvas[:VIEW_H, :VIEW_W] = image

    blob = cv2.dnn.blobFromImage(canvas, scalefactor=1.0, size=(MODEL_SIZE, MODEL_SIZE), swapRB=False, crop=False)

    return blob


def decode(output):
    grids = []
    strides_all = []

    for stride in [8, 16, 32]:
        hsize = MODEL_SIZE // stride
        wsize = MODEL_SIZE // stride

        xv, yv = np.meshgrid(np.arange(wsize), np.arange(hsize))
        grid = np.stack((xv, yv), axis=2).reshape(-1, 2)

        grids.append(grid)
        strides_all.append(np.full((grid.shape[0], 1), stride))

    grids = np.concatenate(grids, axis=0)
    strides_all = np.concatenate(strides_all, axis=0)

    pred = output[0].copy()

    pred[:, :2] = (pred[:, :2] + grids) * strides_all
    pred[:, 2:4] = np.exp(pred[:, 2:4]) * strides_all

    objectness = pred[:, 4]
    class_scores = pred[:, 5:]

    class_ids = np.argmax(class_scores, axis=1)
    scores = objectness * class_scores[np.arange(len(class_ids)), class_ids]

    mask = scores >= SCORE_THRESHOLD

    pred = pred[mask]
    scores = scores[mask]
    class_ids = class_ids[mask]

    boxes = []

    for box in pred[:, :4]:
        cx, cy, bw, bh = box

        x = int(cx - bw / 2)
        y = int(cy - bh / 2)

        boxes.append([x, y, int(bw), int(bh)])

    indices = cv2.dnn.NMSBoxes(boxes, scores.tolist(), SCORE_THRESHOLD, NMS_THRESHOLD)

    results = []

    for i in indices:
        i = int(i)

        results.append({
            "class_id": int(class_ids[i]),
            "score": float(scores[i]),
            "box": boxes[i]
        })

    return results


def test(filename):
    image = cv2.imread(filename)

    if image is None:
        raise RuntimeError(f"Failed to load {filename}")

    blob = prepare(image)

    net.setInput(blob)
    output = net.forward()

    results = decode(output)

    print()
    print("====", filename, "====")

    for result in results:
        class_id = result["class_id"]
        score = result["score"]
        box = result["box"]

        print(f"{COCO_CLASSES[class_id]:15s} score={score:.3f} box={box}")


test("yaw0_1280x960.jpg")
test("yaw0_416x312.jpg")