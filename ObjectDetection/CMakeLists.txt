cmake_minimum_required(VERSION 3.16)
project(cm4_object_detection LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV 5 REQUIRED COMPONENTS core imgproc dnn)
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV dir: ${OpenCV_DIR}")
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET gstreamer-1.0)
pkg_check_modules(GSTREAMER_APP REQUIRED IMPORTED_TARGET gstreamer-app-1.0)
pkg_check_modules(GSTREAMER_VIDEO REQUIRED IMPORTED_TARGET gstreamer-video-1.0)

add_executable(object_detection
    src/main.cpp
    src/rtsp_capture.cpp
    src/dewarp.cpp
    src/yolox_detector.cpp
    src/detection_merge.cpp
)

target_include_directories(object_detection PRIVATE include config)
target_link_libraries(object_detection PRIVATE
    ${OpenCV_LIBS}
    PkgConfig::GSTREAMER
    PkgConfig::GSTREAMER_APP
    PkgConfig::GSTREAMER_VIDEO
)

target_compile_options(object_detection PRIVATE -Wall -Wextra -Wpedantic)
