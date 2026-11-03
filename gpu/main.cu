#include <cuda_runtime.h>

#include "../common/image.h"
#include "../common/render.h"
#include "../common/report.h"
#include "../common/scene.h"

#include <iostream>
#include <vector>

const int ThreadsPerPixel = 16;
const int ThreadsPerBlock = 32;

__global__ void render_kernel(const Scene *scene, Camera camera, RenderSettings settings, const float *texture_data, Framebuffer image) {
    __shared__ Color sample_colors[ThreadsPerBlock];
    // 16 threads for one pixel
    int pixel = blockIdx.x * (ThreadsPerBlock / ThreadsPerPixel) + threadIdx.x / ThreadsPerPixel;
    int sample_lane = threadIdx.x % ThreadsPerPixel;
    int sample_count = settings.camera_samples_x * settings.camera_samples_y;
    Color color{0.0f, 0.0f, 0.0f};

    // each thread trace its sample
    if (pixel < settings.width * settings.height) {
        for (int sample = sample_lane; sample < sample_count; sample += ThreadsPerPixel) {
            Ray ray = ray_for_sample(camera, settings, pixel, sample);
            color = color + trace(ray, *scene, settings, texture_data);
        }
    }
    // all threads wait here, even outside image
    sample_colors[threadIdx.x] = color;
    __syncthreads();

    // add colours in 8, 4, 2, 1 steps
    for (int stride = ThreadsPerPixel / 2; stride > 0; stride /= 2) {
        if (sample_lane < stride) sample_colors[threadIdx.x] = sample_colors[threadIdx.x] + sample_colors[threadIdx.x + stride];
        __syncthreads();
    }

    // first thread store average colour
    if (pixel < settings.width * settings.height && sample_lane == 0) store_pixel(image, pixel, sample_colors[threadIdx.x] / (float)sample_count);
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
    cudaMemcpy(device_texture_data, texture_data.data(), texture_data.size() * sizeof(float), cudaMemcpyHostToDevice);
    Framebuffer device_image{device_pixels, device_pixels + pixel_count, device_pixels + 2 * pixel_count};

    int pixels_per_block = ThreadsPerBlock / ThreadsPerPixel;
    int block_count = (pixel_count + pixels_per_block - 1) / pixels_per_block;
    std::cout << "Kernel                  : render_kernel\n"
              << "Threads per pixel       : " << ThreadsPerPixel << '\n'
              << "Threads per block       : " << ThreadsPerBlock << '\n'
              << "Pixels per block        : " << pixels_per_block << '\n'
              << "Blocks                  : " << block_count << '\n'
              << std::flush;
    for (int run = 0; run < warmup_runs; ++run) {
        render_kernel<<<block_count, ThreadsPerBlock>>>(device_scene, camera, settings, device_texture_data, device_image);
    }
    cudaDeviceSynchronize();

    cudaEvent_t start;
    cudaEvent_t stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);
    cudaEventRecord(start);
    for (int run = 0; run < settings.runs; ++run) {
        render_kernel<<<block_count, ThreadsPerBlock>>>(device_scene, camera, settings, device_texture_data, device_image);
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
