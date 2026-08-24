cmake_minimum_required(VERSION 3.16)
project(rtsp_yolox)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenCV REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED
    gstreamer-1.0
    gstreamer-app-1.0
    gstreamer-video-1.0
)

message(STATUS "OpenCV version: ${OpenCV_VERSION}")

add_executable(rtsp_yolox_round_robin rtsp_yolox_round_robin.cpp)

target_include_directories(rtsp_yolox_round_robin PRIVATE
    ${GSTREAMER_INCLUDE_DIRS}
)

target_link_directories(rtsp_yolox_round_robin PRIVATE
    ${GSTREAMER_LIBRARY_DIRS}
)

target_link_libraries(rtsp_yolox_round_robin PRIVATE
    ${OpenCV_LIBS}
    ${GSTREAMER_LIBRARIES}
)