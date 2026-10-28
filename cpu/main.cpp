#include "../common/image.h"
#include "../common/render.h"
#include "../common/report.h"
#include "../common/scene.h"

#include <chrono>
#include <vector>

int main() {
    RenderSettings settings = configuration();
    const char *image_path = "images/cpu/render.ppm";
    print_report_header("CPU", settings, 0, "steady_clock; serial render loop", image_path);
    std::vector<float> texture_data;
    Scene scene = make_scene(texture_data);
    Camera camera = scene_camera(settings);
    int pixel_count = settings.width * settings.height;
    std::vector<float> pixels(3 * pixel_count);
    Framebuffer image{pixels.data(), pixels.data() + pixel_count, pixels.data() + 2 * pixel_count};

    double total_ms = 0.0;
    for (int run = 0; run < settings.runs; ++run) {
        auto start = std::chrono::steady_clock::now();
        for (int pixel = 0; pixel < pixel_count; ++pixel) {
            Color color = render_pixel(scene, camera, settings, texture_data.data(), pixel);
            store_pixel(image, pixel, color);
        }
        auto stop = std::chrono::steady_clock::now();
        total_ms += std::chrono::duration<double, std::milli>(stop - start).count();
    }
    write_image(image_path, image, settings);
    print_render_time(total_ms / settings.runs);
}
