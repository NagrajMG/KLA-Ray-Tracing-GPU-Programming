#include <cuda_runtime.h>

#include "../common/image.h"
#include "../common/render.h"
#include "../common/report.h"
#include "../common/scene.h"

#include <iostream>
#include <vector>

__global__ void render_kernel(const Scene *scene, Camera camera, RenderSettings settings,
                              const float *texture_data, Framebuffer image) {
    int pixel = blockIdx.x * blockDim.x + threadIdx.x;
    if (pixel >= settings.width * settings.height) {
        return;
    }
    Color color = render_pixel(*scene, camera, settings, texture_data, pixel);
    store_pixel(image, pixel, color);
}

int main() {
    RenderSettings settings = configuration();
    int warmup_runs = 2;
    const char *image_path = "images/gpu/render.ppm";
    print_report_header("GPU", settings, warmup_runs, "CUDA events; render kernels only",
                        image_path);
    std::vector<float> texture_data;
    Scene scene = make_scene(texture_data);
    Camera camera = scene_camera(settings);
    int pixel_count = settings.width * settings.height;
    std::vector<float> pixels(3 * pixel_count);
    Framebuffer image{pixels.data(), pixels.data() + pixel_count, pixels.data() + 2 * pixel_count};

    Scene *device_scene;
    float *device_texture_data;
    float *device_pixels;
    cudaMalloc(&device_scene, sizeof(Scene));
    cudaMalloc(&device_texture_data, texture_data.size() * sizeof(float));
    cudaMalloc(&device_pixels, pixels.size() * sizeof(float));
    cudaMemcpy(device_scene, &scene, sizeof(Scene), cudaMemcpyHostToDevice);
    cudaMemcpy(device_texture_data, texture_data.data(), texture_data.size() * sizeof(float),
               cudaMemcpyHostToDevice);
    Framebuffer device_image{device_pixels, device_pixels + pixel_count,
                             device_pixels + 2 * pixel_count};

    int block_size = 256;
    int block_count = (pixel_count + block_size - 1) / block_size;
    std::cout << "Kernel                  : render_kernel\n"
              << "Threads per block       : " << block_size << '\n'
              << "Blocks                  : " << block_count << '\n'
              << std::flush;
    for (int run = 0; run < warmup_runs; ++run) {
        render_kernel<<<block_count, block_size>>>(device_scene, camera, settings,
                                                   device_texture_data, device_image);
    }
    cudaDeviceSynchronize();

    cudaEvent_t start;
    cudaEvent_t stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);
    cudaEventRecord(start);
    for (int run = 0; run < settings.runs; ++run) {
        render_kernel<<<block_count, block_size>>>(device_scene, camera, settings,
                                                   device_texture_data, device_image);
    }
    cudaEventRecord(stop);
    cudaEventSynchronize(stop);
    float total_ms;
    cudaEventElapsedTime(&total_ms, start, stop);

    cudaMemcpy(pixels.data(), device_pixels, pixels.size() * sizeof(float), cudaMemcpyDeviceToHost);
    write_image(image_path, image, settings);
    print_render_time(total_ms / settings.runs);
    cudaEventDestroy(start);
    cudaEventDestroy(stop);
    cudaFree(device_scene);
    cudaFree(device_texture_data);
    cudaFree(device_pixels);
}
