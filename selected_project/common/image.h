#pragma once

#include "types.h"

#include <cstdio>
#include <filesystem>
#include <vector>

unsigned char display_channel(float value) {
    return (unsigned char)(255.999f * sqrtf(fmaxf(0.0f, fminf(1.0f, value))));
}

void write_image(const char *filename, Framebuffer image, const RenderSettings &settings) {
    std::filesystem::create_directories(std::filesystem::path(filename).parent_path());
    int pixel_count = settings.width * settings.height;
    std::vector<unsigned char> pixels(3 * pixel_count);
    for (int pixel = 0; pixel < pixel_count; ++pixel) {
        pixels[3 * pixel] = display_channel(image.red[pixel]);
        pixels[3 * pixel + 1] = display_channel(image.green[pixel]);
        pixels[3 * pixel + 2] = display_channel(image.blue[pixel]);
    }
    FILE *file = fopen(filename, "wb");
    fprintf(file, "P6\n%d %d\n255\n", settings.width, settings.height);
    fwrite(pixels.data(), 1, pixels.size(), file);
    fclose(file);
}
