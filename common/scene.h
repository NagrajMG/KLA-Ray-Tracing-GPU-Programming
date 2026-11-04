#pragma once

#include "camera.h"
#include "texture.h"

const float RoomHalfWidth = 0.5f * TorusColumns * TorusSpacing + 2.0f;
const float RoomHalfDepth = 0.5f * TorusRows * TorusSpacing + 2.0f;
const float RoomHeight = 14.0f;
const float FloorFront = 30.0f; // Camera at 24.0

RenderSettings configuration() { return {640, 360, 4, 4, 3, 5, 1e-5f}; }

Camera scene_camera(const RenderSettings &settings) {
    return make_camera({0.5f, 9, 24}, {0, 2, -3}, {0, 1, 0}, 62, settings.width, settings.height);
}

float calculate_shininess(float roughness) { return 2.0f / (roughness * roughness) - 2.0f; }

void make_materials(Material *materials) {
    for (int index = 0; index < MaterialCount; index++) {
        materials[index].color_texture = -1; // initial no texture
    }

    Material &floor = materials[FloorMaterial];
    floor.albedo = {0.85f, 0.85f, 0.85f};   // base rgb -- greyish
    floor.specular = {0.05f, 0.05f, 0.05f}; // smaller lighting highlights
    floor.diffuse = 0.90f;                  // more scattering
    floor.reflectivity = 0.06f;
    floor.shininess = calculate_shininess(0.60f);
    floor.color_texture = FloorColor;
    floor.repeat_u = 2.0f * RoomHalfWidth / 8.0f;
    floor.repeat_v = (FloorFront + RoomHalfDepth) / 8.0f;

    // back wall
    Material &wall = materials[WallMaterial];
    wall.albedo = {0.72f, 0.75f, 0.80f};
    wall.specular = {0, 0, 0};
    wall.diffuse = 0.85f;
    wall.reflectivity = 0; // no reflection here
    wall.shininess = calculate_shininess(1.0f);

    // golden torus
    Material &metal = materials[MetalMaterial];
    metal.albedo = {0.82f, 0.50f, 0.10f};
    metal.specular = {0.95f, 0.66f, 0.08f}; // Highly red and green
    metal.diffuse = 0.16f;
    metal.reflectivity = 0.72f; // strength of surrounding
    metal.shininess = calculate_shininess(0.28f);

    // blue torus
    Material &glossy = materials[GlossyCeramicMaterial];
    glossy.albedo = {0.025f, 0.14f, 0.70f};
    glossy.specular = {1, 1, 1}; // no tint
    glossy.diffuse = 0.85f;
    glossy.reflectivity = 0.18f;                   // weak surrounding
    glossy.shininess = calculate_shininess(0.20f); // weak matte

    // red torus
    Material &matte = materials[MatteCeramicMaterial];
    matte.albedo = {0.84f, 0.09f, 0.055f};
    matte.specular = {0, 0, 0}; // zero shininess
    matte.diffuse = 0.90f;
    matte.reflectivity = 0;
    matte.shininess = calculate_shininess(1.0f);

    Material &mirror = materials[MirrorMaterial];
    mirror.albedo = {0, 0, 0};   // no base color
    mirror.specular = {1, 1, 1}; // no tinting
    mirror.diffuse = 0;
    mirror.reflectivity = 0.98f;
    mirror.shininess = calculate_shininess(0.01f);
}

Rectangle make_rectangle(Vec3 corner, Vec3 edge_u, Vec3 edge_v, int material_index) {
    return {corner,
            edge_u,
            edge_v,
            material_index,
            cross(edge_u, edge_v),
            1.0f / dot(edge_u, edge_u),
            1.0f / dot(edge_v, edge_v)};
}

Scene make_scene(std::vector<float> &texture_data) {
    Scene scene{};
    make_materials(scene.materials);
    make_textures(scene.textures, texture_data);

    float tube_radii[3] = {0.16f, 0.26f, 0.38f};
    for (int row = 0; row < TorusRows; ++row) {
        for (int column = 0; column < TorusColumns; ++column) {
            float major_radius = 0.9f + 0.1f * (column % 2);
            float minor_radius = tube_radii[(row + column) % 3];
            TorusOrientation orientation = row < TorusRows / 2 ? Upright : Flat;
            float y = orientation == Flat ? minor_radius : major_radius + minor_radius;
            float x =
                (column - 0.5f * (TorusColumns - 1)) * TorusSpacing + 0.35f * ((row % 2) - 0.5f);
            float z = (row - 0.5f * (TorusRows - 1)) * TorusSpacing;
            int index = row * TorusColumns + column;
            float radius = major_radius + minor_radius;
            scene.tori[index] = {
                {x, y, z}, major_radius,          orientation,          MetalMaterial + index % 3,
                radius,    major_radius / radius, minor_radius / radius};
        }
    }

    scene.rectangles[0] = make_rectangle({-RoomHalfWidth, 0, FloorFront}, {2.0f * RoomHalfWidth, 0, 0}, {0, 0, -FloorFront - RoomHalfDepth}, FloorMaterial);
    scene.rectangles[1] = make_rectangle({-RoomHalfWidth, RoomHeight, -RoomHalfDepth}, {2.0f * RoomHalfWidth, 0, 0}, {0, 0, 2.0f * RoomHalfDepth}, WallMaterial);
    scene.rectangles[2] = make_rectangle({-RoomHalfWidth, 0, RoomHalfDepth}, {0, 0, -2.0f * RoomHalfDepth}, {0, RoomHeight, 0}, MirrorMaterial);
    scene.rectangles[3] = make_rectangle({RoomHalfWidth, 0, -RoomHalfDepth}, {0, 0, 2.0f * RoomHalfDepth}, {0, RoomHeight, 0}, MirrorMaterial);
    scene.rectangles[4] = make_rectangle({-RoomHalfWidth, 0, -RoomHalfDepth}, {2.0f * RoomHalfWidth, 0, 0}, {0, RoomHeight, 0}, WallMaterial);

    float light_y = RoomHeight - 0.2f;
    // warmer light
    scene.lights[0].surface = make_rectangle({-10, light_y, -9}, {6, 0, 0}, {0, 0, 6}, WallMaterial);
    scene.lights[0].emission = {16.0f, 13.5f, 10.5f}; // color emission values of the light

    // cooler light
    scene.lights[1].surface = make_rectangle({4, light_y, -3}, {6, 0, 0}, {0, 0, 6}, WallMaterial);
    scene.lights[1].emission = {10.5f, 12.6f, 14.0f};

    for (AreaLight &light : scene.lights) {
        light.area = length(light.surface.normal);
        light.normal = light.surface.normal / light.area;
        for (int sample = 0; sample < LightSampleCount; ++sample) {
            float u = (sample % LightSamplesX + 0.5f) / LightSamplesX;
            float v = (sample / LightSamplesX + 0.5f) / LightSamplesY;
            light.samples[sample] =
                light.surface.corner + light.surface.edge_u * u + light.surface.edge_v * v;
        }
    }

    return scene;
}
