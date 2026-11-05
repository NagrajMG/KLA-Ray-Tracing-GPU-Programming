#include <cuda_runtime.h>

#include "../common/image.h"
#include "../common/render.h"
#include "../common/report.h"
#include "../common/scene.h"
#include "render.cuh"

#include <iostream>
#include <vector>

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

    int path_count = pixel_count * settings.camera_samples_x * settings.camera_samples_y;
    float *path_values;
    int *path_indices;
    unsigned *path_visibility;
    cudaMalloc(&path_values, (size_t)PathFloatCount * path_count * sizeof(float));
    cudaMalloc(&path_indices, (size_t)PathIntCount * path_count * sizeof(int));
    cudaMalloc(&path_visibility, (size_t)path_count * sizeof(unsigned));
    PathBuffers paths = make_path_buffers(path_values, path_indices, path_visibility, path_count);

    std::cout << "Threads per pixel       : " << ThreadsPerPixel << '\n'
              << "Threads per block       : " << ThreadsPerBlock << '\n'
              << std::flush;
    for (int run = 0; run < warmup_runs; ++run) {
        render_gpu(device_scene, camera, settings, device_texture_data, paths, device_image);
    }
    cudaDeviceSynchronize();

    cudaEvent_t start;
    cudaEvent_t stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);
    cudaEventRecord(start);
    for (int run = 0; run < settings.runs; ++run) {
        render_gpu(device_scene, camera, settings, device_texture_data, paths, device_image);
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
    cudaFree(path_values);
    cudaFree(path_indices);
    cudaFree(path_visibility);
    cudaFree(device_scene);
    cudaFree(device_texture_data);
    cudaFree(device_pixels);
}
