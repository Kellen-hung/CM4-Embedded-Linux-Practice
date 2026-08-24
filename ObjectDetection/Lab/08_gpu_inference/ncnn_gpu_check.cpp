#include <iostream>
#include "gpu.h"

int main()
{
    ncnn::create_gpu_instance();

    int gpu_count = ncnn::get_gpu_count();
    std::cout << "GPU count: " << gpu_count << std::endl;

    for (int i = 0; i < gpu_count; i++) {
        const ncnn::GpuInfo& info = ncnn::get_gpu_info(i);

        std::cout << "GPU " << i << std::endl;
        std::cout << "  name: " << info.device_name() << std::endl;
        std::cout << "  type: " << info.type() << std::endl;
        std::cout << "  driver: " << info.driver_name() << std::endl;
    }

    ncnn::destroy_gpu_instance();
    return 0;
}