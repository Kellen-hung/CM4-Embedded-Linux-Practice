g++ collect_calibration.cpp -o collect_calibration $(pkg-config --cflags --libs gstreamer-1.0 gstreamer-app-1.0 opencv4)

for f in raw/*.jpg; do
    name=$(basename "$f" .jpg)

    echo "Processing $name"

    ffmpeg -loglevel error -y -i "$f" \
        -vf "v360=input=equirect:output=flat:yaw=-120:pitch=0:h_fov=130:v_fov=90:w=1280:h=960" \
        -frames:v 1 "dewarp/left/${name}_left.jpg"

    ffmpeg -loglevel error -y -i "$f" \
        -vf "v360=input=equirect:output=flat:yaw=0:pitch=0:h_fov=130:v_fov=90:w=1280:h=960" \
        -frames:v 1 "dewarp/center/${name}_center.jpg"

    ffmpeg -loglevel error -y -i "$f" \
        -vf "v360=input=equirect:output=flat:yaw=120:pitch=0:h_fov=130:v_fov=90:w=1280:h=960" \
        -frames:v 1 "dewarp/right/${name}_right.jpg"
done

python prepare_calibration.py

find 416 -type f -name "*.png" | sort > calibration.txt

~/ncnn/build/tools/quantize/ncnn2table \
    models/picodet_m_416_opt.param \
    models/picodet_m_416_opt.bin \
    calibration.txt \
    models/picodet_m_416.table \
    mean=[103.53,116.28,123.675] \
    norm=[0.017429,0.017507,0.017125] \
    shape=[416,416,3] \
    pixel=BGR \
    thread=2 \
    method=kl

cd ~/CM4-Embedded-Linux-Practice/ObjectDetection/Lab/06_model_comparison/models

~/ncnn/build/tools/quantize/ncnn2int8 \
    picodet_m_416_opt.param \
    picodet_m_416_opt.bin \
    picodet_m_416_int8.param \
    picodet_m_416_int8.bin \
    picodet_m_416.table