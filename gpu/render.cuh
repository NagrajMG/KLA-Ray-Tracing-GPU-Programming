#pragma once

#include <cuda_runtime.h>

#include "../common/render.h"

const int ThreadsPerPixel = 16;
const int ThreadsPerBlock = 64;
static_assert(ThreadsPerBlock % 32 == 0 && ThreadsPerBlock % ThreadsPerPixel == 0);

const int PathFloatCount = 4 * 3 + 2 * 3 + 2; // four vectors, two colours and UV
const int PathIntCount = 2;                   // material and active flag
const int ShadowSampleCount = LightCount * LightSampleCount;

struct VectorBuffer {
    float *x;
    float *y;
    float *z;

    __device__ Vec3 load(int index) const { return {x[index], y[index], z[index]}; }
    __device__ void store(int index, Vec3 value) {
        x[index] = value.x;
        y[index] = value.y;
        z[index] = value.z;
    }
};

struct PathBuffers {
    VectorBuffer origins;
    VectorBuffer directions;
    VectorBuffer points;
    VectorBuffer normals;
    Framebuffer throughput;
    Framebuffer radiance;
    float *u;
    float *v;
    int *materials;
    int *active;
    unsigned *visibility;
    int count;
};

VectorBuffer take_vectors(float *&data, int count) {
    VectorBuffer result{data, data + count, data + 2 * count};
    data += 3 * count;
    return result;
}

Framebuffer take_colors(float *&data, int count) {
    Framebuffer result{data, data + count, data + 2 * count};
    data += 3 * count;
    return result;
}

PathBuffers make_path_buffers(float *values, int *indices, unsigned *visibility, int count) {
    PathBuffers paths{};
    paths.origins = take_vectors(values, count);
    paths.directions = take_vectors(values, count);
    paths.points = take_vectors(values, count);
    paths.normals = take_vectors(values, count);
    paths.throughput = take_colors(values, count);
    paths.radiance = take_colors(values, count);
    paths.u = values;
    paths.v = values + count;
    paths.materials = indices;
    paths.active = indices + count;
    paths.visibility = visibility;
    paths.count = count;
    return paths;
}

static_assert(ShadowSampleCount == 16, "Visibility packs two 16-sample hits per warp");

__device__ Color load_pixel(Framebuffer image, int index) {
    return {image.red[index], image.green[index], image.blue[index]};
}

__global__ void initialize_kernel(PathBuffers paths, Camera camera, RenderSettings settings) {
    int path = blockIdx.x * blockDim.x + threadIdx.x;
    if (path >= paths.count)
        return;
    int samples = settings.camera_samples_x * settings.camera_samples_y;
    Ray ray = ray_for_sample(camera, settings, path / samples, path % samples);
    paths.origins.store(path, ray.origin);
    paths.directions.store(path, ray.direction);
    store_pixel(paths.throughput, path, {1.0f, 1.0f, 1.0f});
    store_pixel(paths.radiance, path, {0.0f, 0.0f, 0.0f});
    paths.active[path] = 1;
}

__global__ void intersect_kernel(PathBuffers paths, const Scene *device_scene,
                                 RenderSettings settings) {
    int path = blockIdx.x * blockDim.x + threadIdx.x;
    const Scene &scene = *device_scene;
    if (path >= paths.count || !paths.active[path])
        return;
    Ray ray{paths.origins.load(path), paths.directions.load(path)};
    Hit hit = intersect_scene(ray, scene, settings.ray_epsilon, FLT_MAX);
    if (hit.material_index < 0) {
        Color color = load_pixel(paths.radiance, path);
        Color throughput = load_pixel(paths.throughput, path);
        store_pixel(paths.radiance, path, color + throughput * emitted_color(ray, hit, scene));
        paths.active[path] = 0;
        return;
    }
    paths.points.store(path, hit.point);
    paths.normals.store(path, hit.normal);
    paths.u[path] = hit.u;
    paths.v[path] = hit.v;
    paths.materials[path] = hit.material_index;
}

__global__ void shadow_kernel(PathBuffers paths, const Scene *scene, RenderSettings settings) {
    int job = blockIdx.x * blockDim.x + threadIdx.x;
    int path = job / ShadowSampleCount;
    int sample = job % ShadowSampleCount;
    bool visible = false;
    if (path < paths.count && paths.active[path]) {
        Vec3 normal = paths.normals.load(path);
        Vec3 origin = paths.points.load(path) + normal * settings.ray_epsilon;
        const AreaLight &light = scene->lights[sample / LightSampleCount];
        LightSample lighting;
        if (sample_light(origin, normal, light, sample % LightSampleCount, lighting))
            visible = !occluded({origin, lighting.direction}, *scene, settings.ray_epsilon,
                                lighting.distance - settings.ray_epsilon);
    }

    // all lanes vote, including inactive paths and padding
    unsigned mask = __ballot_sync(0xffffffffu, visible);
    if (path < paths.count && sample == 0)
        paths.visibility[path] = (mask >> (threadIdx.x % 32)) & 0xffffu;
}

__global__ void shade_kernel(PathBuffers paths, const Scene *device_scene, RenderSettings settings,
                             const float *texture_data, int depth) {
    int path = blockIdx.x * blockDim.x + threadIdx.x;
    const Scene &scene = *device_scene;
    if (path >= paths.count || !paths.active[path])
        return;
    Hit hit{0.0f,
            paths.points.load(path),
            paths.normals.load(path),
            paths.u[path],
            paths.v[path],
            paths.materials[path],
            -1};
    const Material &material = scene.materials[hit.material_index];
    Vec3 direction = paths.directions.load(path);
    Color albedo = surface_color(hit, material, scene, texture_data);
    Color local = illuminate<false>(hit, albedo, material, direction * -1.0f, scene, settings,
                                    paths.visibility[path]);

    Color color = load_pixel(paths.radiance, path);
    Color throughput = load_pixel(paths.throughput, path);
    store_pixel(paths.radiance, path, color + throughput * local * (1.0f - material.reflectivity));
    if (depth == settings.reflection_depth || material.reflectivity == 0.0f) {
        paths.active[path] = 0;
        return;
    }
    store_pixel(paths.throughput, path, throughput * material.specular * material.reflectivity);
    Ray ray = reflected_ray(hit, direction, settings.ray_epsilon);
    paths.origins.store(path, ray.origin);
    paths.directions.store(path, ray.direction);
}

__global__ void gather_kernel(PathBuffers paths, RenderSettings settings, Framebuffer image) {
    __shared__ Color sample_colors[ThreadsPerBlock];
    int pixel = blockIdx.x * (ThreadsPerBlock / ThreadsPerPixel) + threadIdx.x / ThreadsPerPixel;
    int lane = threadIdx.x % ThreadsPerPixel;
    int sample_count = settings.camera_samples_x * settings.camera_samples_y;
    Color color{0.0f, 0.0f, 0.0f};
    if (pixel < settings.width * settings.height) {
        for (int sample = lane; sample < sample_count; sample += ThreadsPerPixel)
            color = color + load_pixel(paths.radiance, pixel * sample_count + sample);
    }
    sample_colors[threadIdx.x] = color;
    __syncthreads();
    for (int stride = ThreadsPerPixel / 2; stride > 0; stride /= 2) {
        if (lane < stride)
            sample_colors[threadIdx.x] =
                sample_colors[threadIdx.x] + sample_colors[threadIdx.x + stride];
        __syncthreads();
    }
    if (pixel < settings.width * settings.height && lane == 0)
        store_pixel(image, pixel, sample_colors[threadIdx.x] / (float)sample_count);
}

void render_gpu(const Scene *scene, Camera camera, RenderSettings settings,
                const float *texture_data, PathBuffers paths, Framebuffer image) {
    int blocks = (paths.count + ThreadsPerBlock - 1) / ThreadsPerBlock;
    int shadow_blocks = (paths.count * ShadowSampleCount + ThreadsPerBlock - 1) / ThreadsPerBlock;
    int pixels_per_block = ThreadsPerBlock / ThreadsPerPixel;
    int pixel_blocks = (settings.width * settings.height + pixels_per_block - 1) / pixels_per_block;

    initialize_kernel<<<blocks, ThreadsPerBlock>>>(paths, camera, settings);
    for (int depth = 0; depth <= settings.reflection_depth; ++depth) {
        intersect_kernel<<<blocks, ThreadsPerBlock>>>(paths, scene, settings);
        shadow_kernel<<<shadow_blocks, ThreadsPerBlock>>>(paths, scene, settings);
        shade_kernel<<<blocks, ThreadsPerBlock>>>(paths, scene, settings, texture_data, depth);
    }
    gather_kernel<<<pixel_blocks, ThreadsPerBlock>>>(paths, settings, image);
}
