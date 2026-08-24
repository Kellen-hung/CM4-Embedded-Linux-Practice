import cv2
import glob
import os
import numpy as np

MODEL_SIZE = 416

input_files = glob.glob("dewarp/**/*.jpg", recursive=True)
output_dir = "416"
os.makedirs(output_dir, exist_ok=True)

for i, filename in enumerate(sorted(input_files)):
    image = cv2.imread(filename)

    if image is None:
        print("Failed:", filename)
        continue

    h, w = image.shape[:2]

    scale = min(MODEL_SIZE / w, MODEL_SIZE / h)
    new_w = int(w * scale)
    new_h = int(h * scale)

    resized = cv2.resize(image, (new_w, new_h))

    pad_left = (MODEL_SIZE - new_w) // 2
    pad_top = (MODEL_SIZE - new_h) // 2

    letterbox = np.zeros((MODEL_SIZE, MODEL_SIZE, 3), dtype=np.uint8)
    letterbox[pad_top:pad_top + new_h, pad_left:pad_left + new_w] = resized

    output = f"{output_dir}/calib_{i:03d}.png"
    cv2.imwrite(output, letterbox)

print("Prepared:", len(input_files), "images")