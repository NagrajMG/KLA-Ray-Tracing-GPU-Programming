#pragma once

#include "camera.h"
#include "geometry.h"
#include "texture.h"

#include <cfloat>

HD Color surface_color(const Hit &hit, const Material &material, const Scene &scene,
                       const float *texture_data) {
    // base color
    Color color = material.albedo;
    // floors are non-negative
    if (material.color_texture >= 0) {
        float u = hit.u * material.repeat_u;
        float v = hit.v * material.repeat_v;
        Color texel = sample_texture(scene.textures[material.color_texture], texture_data, u, v);
        color = color * texel;
    }
    return color;
}

// does not hit any object
HD Color background(Vec3 direction) {
    // -1 dark, 0 neutral, 1 blue based type
    float blend = 0.5f * (direction.y + 1.0f);
    return Color{0.12f, 0.14f, 0.18f} * (1.0f - blend) + Color{0.38f, 0.50f, 0.70f} * blend;
}

// from 2 area lights including the shadow
HD Color illuminate(const Hit &hit, Color albedo, const Material &material, Vec3 view_direction,
                    const Scene &scene, const RenderSettings &settings) {
    Color color = albedo * (0.03f * material.diffuse);
    Vec3 origin = hit.point + hit.normal * settings.ray_epsilon;
    bool has_specular =
        material.specular.r != 0.0f || material.specular.g != 0.0f || material.specular.b != 0.0f;

    for (int light_index = 0; light_index < LightCount; light_index++) {
        const AreaLight &light = scene.lights[light_index];

        for (int sample = 0; sample < LightSampleCount; sample++) {
            Vec3 to_light = light.samples[sample] - origin;
            float cosine = dot(hit.normal, to_light);
            float light_cosine = -dot(light.normal, to_light);
            if (cosine <= 0.0f || light_cosine <= 0.0f)
                continue;

            float distance_squared = dot(to_light, to_light);
            float distance = sqrtf(distance_squared);
            float inverse_distance = 1.0f / distance;
            Vec3 light_direction = to_light * inverse_distance;
            cosine *= inverse_distance;
            light_cosine *= inverse_distance;

            // check if something blocks the light
            Ray shadow_ray{origin, light_direction};
            if (occluded(shadow_ray, scene, settings.ray_epsilon, distance - settings.ray_epsilon))
                continue;

            Color specular{0.0f, 0.0f, 0.0f};
            if (has_specular) {
                Vec3 half_vector = light_direction + view_direction;
                if (dot(half_vector, half_vector) > 0.0f) { // non zero
                    float alignment = fmaxf(0.0f, dot(hit.normal, normalize(half_vector)));
                    specular = material.specular * powf(alignment, material.shininess);
                }
            }

            Color diffuse = albedo * (material.diffuse * cosine);
            float weight = light.area * light_cosine / (pi * distance_squared * LightSampleCount);
            color = color + (diffuse + specular) * light.emission * weight;
        }
    }

    return color;
}

HD Color trace(Ray ray, const Scene &scene, const RenderSettings &settings,
               const float *texture_data) {
    Color color{0.0f, 0.0f, 0.0f};
    Color throughput{1.0f, 1.0f, 1.0f};

    for (int depth = 0; depth <= settings.reflection_depth; depth++) {
        Hit hit = intersect_scene(ray, scene, settings.ray_epsilon, FLT_MAX);

        if (hit.material_index < 0 && hit.light_index < 0) {
            color = color + throughput * background(ray.direction);
            break;
        }

        // hit by a light rectangle
        if (hit.light_index >= 0) {
            const AreaLight &light = scene.lights[hit.light_index];
            if (dot(ray.direction, light.surface.normal) < 0.0f)
                color = color + throughput * light.emission;
            break;
        }

        const Material &material = scene.materials[hit.material_index];
        Color albedo = surface_color(hit, material, scene, texture_data);
        Color local = illuminate(hit, albedo, material, ray.direction * -1.0f, scene, settings);
        color = color + throughput * local * (1.0f - material.reflectivity);

        if (depth == settings.reflection_depth || material.reflectivity == 0.0f)
            break;

        throughput = throughput * material.specular * material.reflectivity;
        Vec3 direction = reflect(ray.direction, hit.normal);
        ray = {hit.point + hit.normal * settings.ray_epsilon, normalize(direction)};
    }

    return color;
}

HD Color render_pixel(const Scene &scene, const Camera &camera, const RenderSettings &settings,
                      const float *texture_data, int pixel) {
    Color color{0.0f, 0.0f, 0.0f};
    int sample_count = settings.camera_samples_x * settings.camera_samples_y;

    for (int sample = 0; sample < sample_count; sample++) {
        Ray ray = ray_for_sample(camera, settings, pixel, sample);
        color = color + trace(ray, scene, settings, texture_data);
    }

    return color / (float)sample_count;
}

HD void store_pixel(Framebuffer image, int pixel, Color color) {
    image.red[pixel] = color.r;
    image.green[pixel] = color.g;
    image.blue[pixel] = color.b;
}
