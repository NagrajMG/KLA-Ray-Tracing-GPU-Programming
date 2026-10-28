#pragma once

#include <cmath>

#ifdef __CUDACC__
#define HD __host__ __device__
#else
#define HD
#endif

constexpr double pi = 3.14159265358979323846;
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
    double x;
    double y;
    double z;
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
    double major_radius;
    double minor_radius;
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
    double ray_epsilon;
};

struct Material {
    Color albedo;
    Color specular;
    float diffuse;
    float reflectivity;
    float roughness;
    int color_texture;
    double repeat_u;
    double repeat_v;
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
    double t;
    Vec3 point;
    Vec3 normal;
    double u;
    double v;
    int material_index;
    int light_index;
};

HD Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }

HD Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

HD Vec3 operator*(Vec3 a, double scale) { return {a.x * scale, a.y * scale, a.z * scale}; }

HD Vec3 operator/(Vec3 a, double scale) { return a * (1.0 / scale); }

HD double dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

HD Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

HD double length(Vec3 a) { return sqrt(dot(a, a)); }

HD Vec3 normalize(Vec3 a) { return a / length(a); }

HD Vec3 reflect(Vec3 direction, Vec3 normal) {
    return direction - normal * (2.0 * dot(direction, normal));
}

HD Color operator+(Color a, Color b) { return {a.r + b.r, a.g + b.g, a.b + b.b}; }

HD Color operator*(Color a, Color b) { return {a.r * b.r, a.g * b.g, a.b * b.b}; }

HD Color operator*(Color color, float scale) {
    return {color.r * scale, color.g * scale, color.b * scale};
}

HD Color operator/(Color color, float scale) { return color * (1.0f / scale); }
