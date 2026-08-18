import cv2
import numpy as np
import time


# Load model
net = cv2.dnn.readNetFromONNX(
    "models/object_detection_nanodet_2022nov.onnx"
)

# Read image
# image = cv2.imread("./images/rtsp_test.jpg")
image = cv2.imread("./output/center_dewarp.jpg")

if image is None:
    print("Failed to read image")
    exit(1)

# image = image[500:1415, :]

# image = image[:, 0:1280]
# image = image[:, 1280:2560]
# image = image[:, 2560:3840]

# cv2.imwrite("output/cropped.jpg", image)
# cv2.imwrite("output/mid_tile.jpg", image)

def letterbox(srcimg, target_size=(416, 416)):
    img = srcimg.copy()

    h, w = img.shape[:2]

    scale = min(
        target_size[0] / h,
        target_size[1] / w
    )

    new_w = int(w * scale)
    new_h = int(h * scale)

    img = cv2.resize(
        img,
        (new_w, new_h),
        interpolation=cv2.INTER_AREA
    )

    top = (target_size[0] - new_h) // 2
    bottom = target_size[0] - new_h - top

    left = (target_size[1] - new_w) // 2
    right = target_size[1] - new_w - left

    img = cv2.copyMakeBorder(
        img,
        top,
        bottom,
        left,
        right,
        cv2.BORDER_CONSTANT,
        value=0
    )

    return img, (top, left, new_h, new_w)

# Preprocessing
input_image = cv2.cvtColor(
    image,
    cv2.COLOR_BGR2RGB
)

input_image, letterbox_info = letterbox(
    input_image
)

mean = np.array(
    [103.53, 116.28, 123.675],
    dtype=np.float32
)

std = np.array(
    [57.375, 57.12, 58.395],
    dtype=np.float32
)

input_image = input_image.astype(np.float32)
input_image = (input_image - mean) / std

blob = cv2.dnn.blobFromImage(input_image)

net.setInput(blob)


# Get output layer names
output_names = net.getUnconnectedOutLayersNames()

print("Output layers:")
print(output_names)


# Warm up
net.forward(output_names)

start = time.perf_counter()
outputs = net.forward(output_names)
end = time.perf_counter()
ms = (end - start) * 1000
print( ms, "ms")

print("Inference FPS:", 1000 / ms)

# Separate classification outputs and bounding box outputs
class_outputs = []
box_outputs = []

class_names = {
    0: "person",
    15: "cat",
    28: "suitcase",
    62: "tv",
    63: "laptop",
    72: "refrigerator"
}

for output in outputs:
    if output.shape[-1] == 80:
        class_outputs.append(output.squeeze(0))

    elif output.shape[-1] == 32:
        box_outputs.append(output.squeeze(0))


class_scores = np.concatenate(class_outputs, axis=0)
box_data = np.concatenate(box_outputs, axis=0)

print("class_scores shape:", class_scores.shape)
print("box_data shape:", box_data.shape)


# Find best class and confidence of every candidate
class_ids = np.argmax(class_scores, axis=1)
confidences = np.max(class_scores, axis=1)

tv_scores = class_scores[:, 62]

best_tv_index = np.argmax(tv_scores)

print(
    "Best TV score:",
    tv_scores[best_tv_index],
    "candidate:",
    best_tv_index
)


# Decode NanoDet bounding box
def decode_box(candidate_index):
    if candidate_index < 2704:
        stride = 8
        grid_size = 52
        local_index = candidate_index

    elif candidate_index < 3380:
        stride = 16
        grid_size = 26
        local_index = candidate_index - 2704

    else:
        stride = 32
        grid_size = 13
        local_index = candidate_index - 3380

    row = local_index // grid_size
    col = local_index % grid_size

    center_x = col * stride + 0.5 * (stride - 1)
    center_y = row * stride + 0.5 * (stride - 1)

    raw = box_data[candidate_index].reshape(4, 8)

    exp = np.exp(raw)
    prob = exp / np.sum(exp, axis=1, keepdims=True)

    distance = np.dot(prob, np.arange(8)) * stride

    left, top, right, bottom = distance

    x1 = center_x - left
    y1 = center_y - top
    x2 = center_x + right
    y2 = center_y + bottom

    return x1, y1, x2, y2


# Convert 416x416 coordinates back to original image
def scale_box(box):
    x1, y1, x2, y2 = box

    h, w = image.shape[:2]

    top, left, new_h, new_w = letterbox_info

    scale_x = w / new_w
    scale_y = h / new_h

    x1 = int((x1 - left) * scale_x)
    y1 = int((y1 - top) * scale_y)
    x2 = int((x2 - left) * scale_x)
    y2 = int((y2 - top) * scale_y)

    x1 = max(0, min(x1, w - 1))
    y1 = max(0, min(y1, h - 1))
    x2 = max(0, min(x2, w - 1))
    y2 = max(0, min(y2, h - 1))

    return x1, y1, x2, y2


# Keep candidates with confidence >= 0.35
candidate_indices = np.where(
    confidences >= 0.35
)[0]

print("Candidates:", len(candidate_indices))

boxes = []
scores = []

for index in candidate_indices:
    x1, y1, x2, y2 = scale_box(
        decode_box(index)
    )

    # NMSBoxes 要的是 x, y, width, height
    boxes.append([
        x1,
        y1,
        x2 - x1,
        y2 - y1
    ])

    scores.append(
        float(confidences[index])
    )


indices = cv2.dnn.NMSBoxes(
    boxes,
    scores,
    score_threshold=0.35,
    nms_threshold=0.4
)

indices = np.array(indices).reshape(-1)

print("After NMS:", len(indices))

for i in indices:
    index = candidate_indices[i]

    class_id = int(class_ids[index])
    confidence = float(confidences[index])

    x1, y1, x2, y2 = scale_box(
        decode_box(index)
    )

    name = class_names.get(class_id, str(class_id))

    label = f"{name} {confidence:.2f}"

    cv2.rectangle(
        image,
        (x1, y1),
        (x2, y2),
        (0, 255, 0),
        3
    )

    cv2.putText(
        image,
        label,
        (x1, max(y1 - 10, 20)),
        cv2.FONT_HERSHEY_SIMPLEX,
        1,
        (0, 255, 0),
        2
    )

    print(label, (x1, y1, x2, y2))

cv2.imwrite("output/rtsp_result.jpg", image)