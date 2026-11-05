#include <cuda_runtime.h>

#include "../common/image.h"
#include "../common/render.h"
#include "../common/report.h"
#include "../common/scene.h"

#include <iostream>
#include <cstdlib>
#include <vector>

const int ThreadsPerPixel = 16;
const int ThreadsPerBlock = 32;

__global__ void render_kernel(const Scene *scene, Camera camera, RenderSettings settings, const ColorScalar *texture_data, Framebuffer image) {
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

void cuda_check(cudaError_t result, const char *operation) {
    if (result != cudaSuccess) {
        std::cerr << operation << ": " << cudaGetErrorString(result) << '\n';
        std::exit(EXIT_FAILURE);
    }
}

int main(int argc, char **argv) {
    if (argc > 3) {
        std::cerr << "Usage: " << argv[0] << " [image.ppm [linear-framebuffer.bin]]\n";
        return EXIT_FAILURE;
    }
    int device;
    cuda_check(cudaGetDevice(&device), "Get CUDA device");
    cudaDeviceProp properties;
    cuda_check(cudaGetDeviceProperties(&properties, device), "Get CUDA device properties");
    std::cout << "CUDA device             : " << properties.name << '\n'
              << "Compute capability      : " << properties.major << '.' << properties.minor << '\n';
    RenderSettings settings = configuration();
    int warmup_runs = 2;
#if defined(RT_FP16_GEOMETRY)
    const char *default_image_path = "images/gpu/render_fp16_geometry.ppm";
#elif defined(RT_FP16_COLORS)
    const char *default_image_path = "images/gpu/render_fp16_color.ppm";
#else
    const char *default_image_path = "images/gpu/render.ppm";
#endif
    const char *image_path = argc > 1 ? argv[1] : default_image_path;
    print_report_header("GPU", settings, warmup_runs, "CUDA events; render kernels only",
                        image_path);
    std::vector<ColorScalar> texture_data;
    Scene scene = make_scene(texture_data);
    Camera camera = scene_camera(settings);
    int pixel_count = settings.width * settings.height;
    std::vector<ColorScalar> pixels(3 * pixel_count);
    Framebuffer image{pixels.data(), pixels.data() + pixel_count, pixels.data() + 2 * pixel_count};

    Scene *device_scene;
    ColorScalar *device_texture_data;
    ColorScalar *device_pixels;
    cuda_check(cudaMalloc(&device_scene, sizeof(Scene)), "cudaMalloc");
    cuda_check(cudaMalloc(&device_texture_data, texture_data.size() * sizeof(ColorScalar)), "cudaMalloc");
    cuda_check(cudaMalloc(&device_pixels, pixels.size() * sizeof(ColorScalar)), "cudaMalloc");
    cuda_check(cudaMemcpy(device_scene, &scene, sizeof(Scene), cudaMemcpyHostToDevice), "cudaMemcpy");
    cuda_check(cudaMemcpy(device_texture_data, texture_data.data(), texture_data.size() * sizeof(ColorScalar), cudaMemcpyHostToDevice), "cudaMemcpy");
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
        cuda_check(cudaGetLastError(), "Launch render_kernel");
    }
    cuda_check(cudaDeviceSynchronize(), "cudaDeviceSynchronize");

    cudaEvent_t start;
    cudaEvent_t stop;
    cuda_check(cudaEventCreate(&start), "cudaEventCreate");
    cuda_check(cudaEventCreate(&stop), "cudaEventCreate");
    cuda_check(cudaEventRecord(start), "cudaEventRecord");
    for (int run = 0; run < settings.runs; ++run) {
        render_kernel<<<block_count, ThreadsPerBlock>>>(device_scene, camera, settings, device_texture_data, device_image);
        cuda_check(cudaGetLastError(), "Launch render_kernel");
    }
    cuda_check(cudaEventRecord(stop), "cudaEventRecord");
    cuda_check(cudaEventSynchronize(stop), "cudaEventSynchronize");
    float total_ms;
    cuda_check(cudaEventElapsedTime(&total_ms, start, stop), "cudaEventElapsedTime");

    cuda_check(cudaMemcpy(pixels.data(), device_pixels, pixels.size() * sizeof(ColorScalar), cudaMemcpyDeviceToHost), "cudaMemcpy");
    write_image(image_path, image, settings);
    if (argc > 2) {
        write_linear_image(argv[2], image, settings);
        std::cout << "Linear framebuffer      : " << argv[2] << '\n';
    }
    size_t nonfinite = 0;
    for (ColorScalar value : pixels)
        nonfinite += !std::isfinite(float(value));
    std::cout << "Non-finite channels     : " << nonfinite << '\n';
    print_render_time(total_ms / settings.runs);
    cuda_check(cudaEventDestroy(start), "cudaEventDestroy");
    cuda_check(cudaEventDestroy(stop), "cudaEventDestroy");
    cuda_check(cudaFree(device_scene), "cudaFree");
    cuda_check(cudaFree(device_texture_data), "cudaFree");
    cuda_check(cudaFree(device_pixels), "cudaFree");
}
