g++ RTSP-capture.cpp -o RTSP-capture \
    $(pkg-config --cflags --libs gstreamer-1.0 gstreamer-app-1.0)

g++ RTSP-capture.cpp -o RTSP-capture \
    $(pkg-config --cflags --libs gstreamer-1.0 gstreamer-app-1.0 opencv4)