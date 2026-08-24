#include <iostream>
#include <chrono>
#include <string>

#include "net.h"
#include "gpu.h"

static int benchmark(const std::string& param_path, const std::string& bin_path, bool use_gpu)
{
    ncnn::Net net;

    net.opt.use_vulkan_compute = use_gpu;
    net.opt.num_threads = 4;

    if (use_gpu)
        net.set_vulkan_device(0);

    if (net.load_param(param_path.c_str()) != 0) {
        std::cerr << "load_param failed" << std::endl;
        return -1;
    }

    if (net.load_model(bin_path.c_str()) != 0) {
        std::cerr << "load_model failed" << std::endl;
        return -1;
    }

    // YOLOX-Nano input: 416x416x3
    // 先用固定假資料，只驗證 inference path 與 timing。
    ncnn::Mat input(416, 416, 3);
    input.fill(114.f);

    // Warm-up，尤其 Vulkan 第一次執行可能包含額外初始化成本
    for (int i = 0; i < 5; i++) {
        ncnn::Extractor ex = net.create_extractor();
        ex.input("in0", input);

        ncnn::Mat out;
        if (ex.extract("out0", out) != 0) {
            std::cerr << "warm-up extract failed" << std::endl;
            return -1;
        }
    }

    const int runs = 20;
    double total_ms = 0.0;

    ncnn::Mat last_out;

    for (int i = 0; i < runs; i++) {
        ncnn::Extractor ex = net.create_extractor();
        ex.input("in0", input);

        auto start = std::chrono::steady_clock::now();

        ncnn::Mat out;
        int ret = ex.extract("out0", out);

        auto end = std::chrono::steady_clock::now();

        if (ret != 0) {
            std::cerr << "extract failed: " << ret << std::endl;
            return -1;
        }

        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        total_ms += ms;
        last_out = out;

        std::cout << (use_gpu ? "GPU" : "CPU")
                  << " run " << i + 1
                  << ": " << ms << " ms" << std::endl;
    }

    double avg_ms = total_ms / runs;

    std::cout << "\n=== " << (use_gpu ? "Vulkan / V3D" : "CPU") << " ===" << std::endl;
    std::cout << "output dims: " << last_out.dims << std::endl;
    std::cout << "output w:    " << last_out.w << std::endl;
    std::cout << "output h:    " << last_out.h << std::endl;
    std::cout << "output c:    " << last_out.c << std::endl;
    std::cout << "average:     " << avg_ms << " ms" << std::endl;
    std::cout << "FPS:         " << 1000.0 / avg_ms << std::endl;

    return 0;
}

int main()
{
    const std::string param_path = "models/yolox-nano/yolox_nano.ncnn.param";
    const std::string bin_path = "models/yolox-nano/yolox_nano.ncnn.bin";

    ncnn::create_gpu_instance();

    std::cout << "\n######## CPU ########\n" << std::endl;
    if (benchmark(param_path, bin_path, false) != 0)
        return -1;

    std::cout << "\n######## VULKAN ########\n" << std::endl;
    if (benchmark(param_path, bin_path, true) != 0)
        return -1;

    ncnn::destroy_gpu_instance();

    return 0;
}