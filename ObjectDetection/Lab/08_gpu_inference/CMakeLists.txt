cmake_minimum_required(VERSION 3.10)

project(ncnn_gpu_check)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ncnn_DIR "/home/admin/ncnn/build-vulkan/install/lib/cmake/ncnn")

find_package(ncnn REQUIRED)

# add_executable(ncnn_gpu_check ncnn_gpu_check.cpp)
# target_link_libraries(ncnn_gpu_check ncnn)

add_executable(benchmark_yolox_ncnn benchmark_yolox_ncnn.cpp)
target_link_libraries(benchmark_yolox_ncnn ncnn)