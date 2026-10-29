#pragma once

#include <cmath>

#ifdef __CUDACC__
#define HD __host__ __device__
#else
#define HD
#endif

constexpr float pi = 3.14159265358979323846f;
const int TorusRows = 8;
const int TorusColumns = 8;
const int TorusCount = TorusRows * TorusColumns;
const int RectangleCount = 5;
const int LightCount = 2;

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
    float x;
    float y;
    float z;
};

struct Color {
    float r;
    float g;
    float b;
};

struct Ray {
    Vec3 origin;
    Vec3 direction;
};

enum TorusOrientation { Flat, Upright };

struct Torus {
    Vec3 center;
    float major_radius;
    float minor_radius;
    TorusOrientation orientation;
    int material_index;
};

struct Rectangle {
    Vec3 corner;
    Vec3 edge_u;
    Vec3 edge_v;
    int material_index;
};

struct Texture {
    int width;
    int height;
};

struct Framebuffer {
    float *red;
    float *green;
    float *blue;
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
    int light_samples_x;
    int light_samples_y;
    int reflection_depth;
    int runs;
    float ray_epsilon;
};

struct Material {
    Color albedo;
    Color specular;
    float diffuse;
    float reflectivity;
    float roughness;
    int color_texture;
    float repeat_u;
    float repeat_v;
};

struct AreaLight {
    Rectangle surface;
    Color emission;
};

struct Scene {
    Torus tori[TorusCount];
    Rectangle rectangles[RectangleCount];
    Material materials[MaterialCount];
    AreaLight lights[LightCount];
    Texture textures[TextureCount];
};

struct Hit {
    float t;
    Vec3 point;
    Vec3 normal;
    float u;
    float v;
    int material_index;
    int light_index;
};

HD Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }

HD Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

HD Vec3 operator*(Vec3 a, float scale) { return {a.x * scale, a.y * scale, a.z * scale}; }

HD Vec3 operator/(Vec3 a, float scale) { return a * (1.0f / scale); }

HD float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

HD Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

HD float length(Vec3 a) { return sqrtf(dot(a, a)); }

HD Vec3 normalize(Vec3 a) { return a / length(a); }

HD Vec3 reflect(Vec3 direction, Vec3 normal) {
    return direction - normal * (2.0f * dot(direction, normal));
}

HD Color operator+(Color a, Color b) { return {a.r + b.r, a.g + b.g, a.b + b.b}; }

HD Color operator*(Color a, Color b) { return {a.r * b.r, a.g * b.g, a.b * b.b}; }

HD Color operator*(Color color, float scale) {
    return {color.r * scale, color.g * scale, color.b * scale};
}

HD Color operator/(Color color, float scale) { return color * (1.0f / scale); }
