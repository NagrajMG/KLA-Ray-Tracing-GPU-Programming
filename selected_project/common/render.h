#pragma once

#include "camera.h"
#include "geometry.h"
#include "texture.h"

#include <cfloat>

HD Color surface_color(const Hit &hit, const Material &material, const Scene &scene,
                       const float *texture_data) {
    Color color = material.albedo;
    if (material.color_texture >= 0) {
        double u = hit.u * material.repeat_u;
        double v = hit.v * material.repeat_v;
        Color texel = sample_texture(scene.textures[material.color_texture], texture_data, u, v);
        color = color * texel;
    }
    return color;
}

HD Color background(Vec3 direction) {
    float blend = (float)(0.5 * (direction.y + 1.0));
    return Color{0.12f, 0.14f, 0.18f} * (1.0f - blend) + Color{0.38f, 0.50f, 0.70f} * blend;
}

HD Color illuminate(const Hit &hit, Color albedo, const Material &material, Vec3 view_direction,
                    const Scene &scene, const RenderSettings &settings) {
    Color color = albedo * (0.03f * material.diffuse);
    Vec3 origin = hit.point + hit.normal * settings.ray_epsilon;
    int sample_count = settings.light_samples_x * settings.light_samples_y;
    float shininess = 2.0f / (material.roughness * material.roughness) - 2.0f;

    for (int light_index = 0; light_index < LightCount; light_index++) {
        const AreaLight &light = scene.lights[light_index];
        Vec3 area_vector = cross(light.surface.edge_u, light.surface.edge_v);
        double area = length(area_vector);
        Vec3 light_normal = area_vector / area;

        for (int sample = 0; sample < sample_count; sample++) {
            double u = (sample % settings.light_samples_x + 0.5) / settings.light_samples_x;
            double v = (sample / settings.light_samples_x + 0.5) / settings.light_samples_y;
            Vec3 target =
                light.surface.corner + light.surface.edge_u * u + light.surface.edge_v * v;
            Vec3 to_light = target - origin;
            double distance_squared = dot(to_light, to_light);
            double distance = sqrt(distance_squared);
            Vec3 light_direction = to_light / distance;
            double cosine = dot(hit.normal, light_direction);
            double light_cosine = -dot(light_normal, light_direction);

            if (cosine <= 0.0 || light_cosine <= 0.0)
                continue;

            Ray shadow_ray{origin, light_direction};
            Hit blocker = intersect_scene(shadow_ray, scene, settings.ray_epsilon,
                                          distance - settings.ray_epsilon);
            if (blocker.material_index >= 0 || blocker.light_index >= 0)
                continue;

            Vec3 half_vector = light_direction + view_direction;
            float highlight = 0.0f;
            if (dot(half_vector, half_vector) > 0.0) {
                float alignment = (float)fmax(0.0, dot(hit.normal, normalize(half_vector)));
                highlight = powf(alignment, shininess);
            }

            Color diffuse = albedo * (material.diffuse * (float)cosine);
            Color specular = material.specular * highlight;
            float weight = (float)(area * light_cosine / (pi * distance_squared * sample_count));
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
        Hit hit = intersect_scene(ray, scene, settings.ray_epsilon, DBL_MAX);

        if (hit.material_index < 0 && hit.light_index < 0) {
            color = color + throughput * background(ray.direction);
            break;
        }

        if (hit.light_index >= 0) {
            const AreaLight &light = scene.lights[hit.light_index];
            Vec3 normal = cross(light.surface.edge_u, light.surface.edge_v);
            if (dot(ray.direction, normal) < 0.0)
                color = color + throughput * light.emission;
            break;
        }

        const Material &material = scene.materials[hit.material_index];
        Color albedo = surface_color(hit, material, scene, texture_data);
        Color local = illuminate(hit, albedo, material, ray.direction * -1.0, scene, settings);
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
