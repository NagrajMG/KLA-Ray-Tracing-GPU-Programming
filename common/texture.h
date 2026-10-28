#pragma once

#include "types.h"

#include <vector>

HD Color texture_texel(const Texture &texture, const float *data, int x, int y) {
    int offset = (y * texture.width + x) * 3;
    return {data[offset], data[offset + 1], data[offset + 2]};
}

HD Color sample_texture(const Texture &texture, const float *data, double u, double v) {
    double x = (u - floor(u)) * texture.width - 0.5;
    double y = (v - floor(v)) * texture.height - 0.5;
    int left = (int)floor(x);
    int top = (int)floor(y);
    float fraction_x = (float)(x - left);
    float fraction_y = (float)(y - top);
    left = (left + texture.width) % texture.width;
    top = (top + texture.height) % texture.height;
    int right = (left + 1) % texture.width;
    int bottom = (top + 1) % texture.height;
    Color upper = texture_texel(texture, data, left, top) * (1.0f - fraction_x) +
                  texture_texel(texture, data, right, top) * fraction_x;
    Color lower = texture_texel(texture, data, left, bottom) * (1.0f - fraction_x) +
                  texture_texel(texture, data, right, bottom) * fraction_x;
    return upper * (1.0f - fraction_y) + lower * fraction_y;
}

void make_textures(Texture *textures, std::vector<float> &data) {
    Texture &texture = textures[FloorColor];
    texture = {512, 512};
    data.resize(texture.width * texture.height * 3);
    for (int y = 0; y < texture.height; ++y) {
        for (int x = 0; x < texture.width; ++x) {
            int checker = (8 * x / texture.width + 8 * y / texture.height) % 2;
            Color value = checker ? Color{0.14f, 0.17f, 0.20f} : Color{0.78f, 0.73f, 0.63f};
            int entry = (y * texture.width + x) * 3;
            data[entry] = value.r;
            data[entry + 1] = value.g;
            data[entry + 2] = value.b;
        }
    }
}
