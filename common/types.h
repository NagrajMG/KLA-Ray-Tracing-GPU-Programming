#pragma once

#include "precision.h"

constexpr float pi = 3.14159265358979323846f;
const int TorusRows = 8;
const int TorusColumns = 8;
const int TorusCount = TorusRows * TorusColumns;
const float TorusSpacing = 3.3f;
const int RectangleCount = 5;
const int LightCount = 2;
const int LightSamplesX = 4;
const int LightSamplesY = 2;
const int LightSampleCount = LightSamplesX * LightSamplesY;

enum MaterialIndex {
    FloorMaterial,
    WallMaterial,
    MetalMaterial,
    GlossyCeramicMaterial,
    MatteCeramicMaterial,
    MirrorMaterial,
    MaterialCount
};

enum TextureIndex { FloorColor, TextureCount };

struct Vec3 {
    Real x;
    Real y;
    Real z;
};

struct Color {
    ColorScalar r;
    ColorScalar g;
    ColorScalar b;
};

struct Ray {
    Vec3 origin;
    Vec3 direction;
};

enum TorusOrientation { Flat, Upright };

struct Torus {
    Vec3 center;
    Real major_radius;
    TorusOrientation orientation;
    int material_index;
    Real radius;
    Real scaled_major_radius;
    Real scaled_minor_radius;
};

struct Rectangle {
    Vec3 corner;
    Vec3 edge_u;
    Vec3 edge_v;
    int material_index;
    Vec3 normal;
    Real inverse_u_squared;
    Real inverse_v_squared;
};

struct Texture {
    int width;
    int height;
};

struct Framebuffer {
    ColorScalar *red;
    ColorScalar *green;
    ColorScalar *blue;
};

struct Camera {
    Vec3 origin;
    Vec3 pixel00;
    Vec3 pixel_du;
    Vec3 pixel_dv;
};

struct RenderSettings {
    int width;
    int height;
    int camera_samples_x;
    int camera_samples_y;
    int reflection_depth;
    int runs;
    Real ray_epsilon;
};

struct Material {
    Color albedo;
    Color specular;
    ColorScalar diffuse;
    ColorScalar reflectivity;
    int color_texture;
    Real repeat_u;
    Real repeat_v;
    ColorScalar shininess;
};

struct AreaLight {
    Rectangle surface;
    Color emission;
    Real area;
    Vec3 normal;
    Vec3 samples[LightSampleCount];
};

struct Scene {
    Torus tori[TorusCount];
    Real torus_min_y;
    Real torus_max_y;
    Rectangle rectangles[RectangleCount];
    Material materials[MaterialCount];
    AreaLight lights[LightCount];
    Texture textures[TextureCount];
};

struct Hit {
    Real t;
    Vec3 point;
    Vec3 normal;
    Real u;
    Real v;
    int material_index;
    int light_index;
};

HD Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }

HD Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

HD Vec3 operator*(Vec3 a, Real scale) { return {a.x * scale, a.y * scale, a.z * scale}; }

HD Vec3 operator/(Vec3 a, Real scale) { return a * (1.0f / scale); }

HD Real dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

HD Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

HD Real length(Vec3 a) { return real_sqrt(dot(a, a)); }

HD Vec3 normalize(Vec3 a) {
#ifdef RT_FP16_GEOMETRY
    // Room normals are unnormalized cross products. Squaring their components
    // in FP16 overflows, so calculate normalization in FP32 and store FP16.
    float x = float(a.x);
    float y = float(a.y);
    float z = float(a.z);
    float squared_length = x * x + y * y + z * z;
    if (squared_length == 0.0f)
        return a;
    float inverse_length = 1.0f / sqrtf(squared_length);
    return {Real(x * inverse_length), Real(y * inverse_length), Real(z * inverse_length)};
#else
    return a / length(a);
#endif
}

HD Vec3 reflect(Vec3 direction, Vec3 normal) {
    return direction - normal * (2.0f * dot(direction, normal));
}

HD Color operator+(Color a, Color b) {
#if defined(RT_FP16_COLORS) && defined(__CUDA_ARCH__)
    __half2 rg = __hadd2(__halves2half2(a.r.value, a.g.value),
                        __halves2half2(b.r.value, b.g.value));
    return {Half::from_native(__low2half(rg)), Half::from_native(__high2half(rg)), a.b + b.b};
#else
    return {a.r + b.r, a.g + b.g, a.b + b.b};
#endif
}

HD Color operator*(Color a, Color b) {
#if defined(RT_FP16_COLORS) && defined(__CUDA_ARCH__)
    __half2 rg = __hmul2(__halves2half2(a.r.value, a.g.value),
                        __halves2half2(b.r.value, b.g.value));
    return {Half::from_native(__low2half(rg)), Half::from_native(__high2half(rg)), a.b * b.b};
#else
    return {a.r * b.r, a.g * b.g, a.b * b.b};
#endif
}

HD Color operator*(Color color, ColorScalar scale) {
    return color * Color{scale, scale, scale};
}

HD Color operator/(Color color, ColorScalar scale) { return color * (1.0f / scale); }
