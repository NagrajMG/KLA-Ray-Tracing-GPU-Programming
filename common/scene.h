#pragma once

#include "camera.h"
#include "texture.h"

const float TorusSpacing = 3.3f;
const float RoomHalfWidth = 0.5f * TorusColumns * TorusSpacing + 2.0f;
const float RoomHalfDepth = 0.5f * TorusRows * TorusSpacing + 2.0f;
const float RoomHeight = 14.0f;
const float FloorFront = 30.0f; // Camera at 24.0

RenderSettings configuration() { return {640, 360, 4, 4, 4, 2, 3, 5, 1e-5f}; }

Camera scene_camera(const RenderSettings &settings) {
    return make_camera({0.5f, 9, 24}, {0, 2, -3}, {0, 1, 0}, 62, settings.width, settings.height);
}

void make_materials(Material *materials) {
    for (int index = 0; index < MaterialCount; index++) {
        materials[index].color_texture = -1; // initial no texture
    }

    Material &floor = materials[FloorMaterial];
    floor.albedo = {0.85f, 0.85f, 0.85f}; // base rgb -- greyish
    floor.specular = {0.05f, 0.05f, 0.05f}; // smaller lighting highlights
    floor.diffuse = 0.90f; // more scattering
    floor.reflectivity = 0.06f; 
    floor.roughness = 0.60f;
    floor.color_texture = FloorColor;
    floor.repeat_u = 2.0f * RoomHalfWidth / 8.0f;
    floor.repeat_v = (FloorFront + RoomHalfDepth) / 8.0f;

    // back wall
    Material &wall = materials[WallMaterial];
    wall.albedo = {0.72f, 0.75f, 0.80f};
    wall.specular = {0, 0, 0};
    wall.diffuse = 0.85f;
    wall.reflectivity = 0; // no reflection here
    wall.roughness = 1; 

    // golden torus
    Material &metal = materials[MetalMaterial];
    metal.albedo = {0.82f, 0.50f, 0.10f};
    metal.specular = {0.95f, 0.66f, 0.08f}; // Highly red and green
    metal.diffuse = 0.16f;
    metal.reflectivity = 0.72f; // strength of surrounding
    metal.roughness = 0.28f;

    // blue torus
    Material &glossy = materials[GlossyCeramicMaterial];
    glossy.albedo = {0.025f, 0.14f, 0.70f};
    glossy.specular = {1, 1, 1}; // no tint
    glossy.diffuse = 0.85f;
    glossy.reflectivity = 0.18f; // weak surrounding
    glossy.roughness = 0.20f; // weak matte

    // red torus
    Material &matte = materials[MatteCeramicMaterial];
    matte.albedo = {0.84f, 0.09f, 0.055f};
    matte.specular = {0, 0, 0}; // zero shininess
    matte.diffuse = 0.90f;
    matte.reflectivity = 0;
    matte.roughness = 1;

    Material &mirror = materials[MirrorMaterial];
    mirror.albedo = {0, 0, 0}; // no base color
    mirror.specular = {1, 1, 1}; // no tinting
    mirror.diffuse = 0;
    mirror.reflectivity = 0.98f;
    mirror.roughness = 0.01f;
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
            float x = (column - 0.5f * (TorusColumns - 1)) * TorusSpacing + 0.35f * ((row % 2) - 0.5f);
            float z = (row - 0.5f * (TorusRows - 1)) * TorusSpacing;
            int index = row * TorusColumns + column;
            scene.tori[index] = {
                {x, y, z}, major_radius, minor_radius, orientation, MetalMaterial + index % 3};
        }
    }

    scene.rectangles[0] = {{-RoomHalfWidth, 0, FloorFront},
                           {2.0f * RoomHalfWidth, 0, 0},
                           {0, 0, -FloorFront - RoomHalfDepth},
                           FloorMaterial};
    scene.rectangles[1] = {{-RoomHalfWidth, RoomHeight, -RoomHalfDepth},
                           {2.0f * RoomHalfWidth, 0, 0},
                           {0, 0, 2.0f * RoomHalfDepth},
                           WallMaterial};
    scene.rectangles[2] = {{-RoomHalfWidth, 0, RoomHalfDepth},
                           {0, 0, -2.0f * RoomHalfDepth},
                           {0, RoomHeight, 0},
                           MirrorMaterial};
    scene.rectangles[3] = {{RoomHalfWidth, 0, -RoomHalfDepth},
                           {0, 0, 2.0f * RoomHalfDepth},
                           {0, RoomHeight, 0},
                           MirrorMaterial};
    scene.rectangles[4] = {{-RoomHalfWidth, 0, -RoomHalfDepth},
                           {2.0f * RoomHalfWidth, 0, 0},
                           {0, RoomHeight, 0},
                           WallMaterial};

    float light_y = RoomHeight - 0.2f;
    // warmer light
    scene.lights[0] = {{{-10, light_y, -9}, {6, 0, 0}, {0, 0, 6}, WallMaterial},
                       {16.0f, 13.5f, 10.5f}}; // color emission values of the light

    // cooler light
    scene.lights[1] = {{{4, light_y, -3}, {6, 0, 0}, {0, 0, 6}, WallMaterial},
                       {10.5f, 12.6f, 14.0f}};

    return scene;
}
