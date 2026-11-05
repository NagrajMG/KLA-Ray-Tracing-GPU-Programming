#pragma once

#include "types.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

unsigned char display_channel(float value) {
    return (unsigned char)(255.999f * sqrtf(fmaxf(0.0f, fminf(1.0f, value))));
}

void write_image(const char *filename, Framebuffer image, const RenderSettings &settings) {
    auto directory = std::filesystem::path(filename).parent_path();
    if (!directory.empty())
        std::filesystem::create_directories(directory);
    int pixel_count = settings.width * settings.height;
    std::vector<unsigned char> pixels(3 * pixel_count);
    for (int pixel = 0; pixel < pixel_count; ++pixel) {
        pixels[3 * pixel] = display_channel(image.red[pixel]);
        pixels[3 * pixel + 1] = display_channel(image.green[pixel]);
        pixels[3 * pixel + 2] = display_channel(image.blue[pixel]);
    }
    FILE *file = fopen(filename, "wb");
    if (!file)
        throw std::runtime_error("Failed to open display image");
    fprintf(file, "P6\n%d %d\n255\n", settings.width, settings.height);
    size_t written = fwrite(pixels.data(), 1, pixels.size(), file);
    int closed = fclose(file);
    if (written != pixels.size() || closed != 0)
        throw std::runtime_error("Failed to write display image");
}

// Headerless planar linear RGB, using ColorScalar's native representation.
// Keep this alongside the display image to detect NaNs and quantify FP16 error.
void write_linear_image(const char *filename, Framebuffer image, const RenderSettings &settings) {
    auto directory = std::filesystem::path(filename).parent_path();
    if (!directory.empty())
        std::filesystem::create_directories(directory);
    std::ofstream file(filename, std::ios::binary);
    auto bytes = settings.width * settings.height * sizeof(ColorScalar);
    file.write(reinterpret_cast<const char *>(image.red), bytes);
    file.write(reinterpret_cast<const char *>(image.green), bytes);
    file.write(reinterpret_cast<const char *>(image.blue), bytes);
    if (!file)
        throw std::runtime_error("Failed to write linear framebuffer");
}
