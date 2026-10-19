#include <cuda_runtime.h>
#include <cmath>
#include <cstdio>
#include <vector>

struct Vec3 {
    float x, y, z;
    __host__ __device__ Vec3(float a = 0, float b = 0, float c = 0) : x(a), y(b), z(c) {}
};

__host__ __device__ Vec3 operator+(Vec3 a, Vec3 b) {
    return Vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

__host__ __device__ Vec3 operator-(Vec3 a, Vec3 b) {
    return Vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

__host__ __device__ Vec3 operator*(Vec3 a, float s) {
    return Vec3(a.x * s, a.y * s, a.z * s);
}

__host__ __device__ float dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

__host__ __device__ Vec3 normalize(Vec3 a) {
    float len = sqrtf(dot(a, a));
    return a * (1.0f / len);
}

struct Sphere {
    Vec3 center;
    float radius;
    Vec3 color;
    bool isFloor;
};

__device__ float hitSphere(const Sphere& s, Vec3 origin, Vec3 dir) {
    Vec3 oc = origin - s.center;
    float a = dot(dir, dir);
    float b = 2.0f * dot(oc, dir);
    float c = dot(oc, oc) - s.radius * s.radius;
    float disc = b * b - 4 * a * c;
    if (disc < 0) return -1.0f;
    float t = (-b - sqrtf(disc)) / (2 * a);
    return (t > 0.001f) ? t : -1.0f;
}

// There are multiple spheres, so need to get the closest hit
__device__ int closestHit(Vec3 origin, Vec3 dir, float& tHit,
                          const Sphere* scene, int sphereCount) {
    int best = -1;
    float closest = 1e30f;
    for (int i = 0; i < sphereCount; i++) {
        float t = hitSphere(scene[i], origin, dir);
        if (t > 0 && t < closest) {
            closest = t;
            best = i;
        }
    }
    tHit = closest;
    return best;
}

__device__ Vec3 rayColor(Vec3 origin, Vec3 dir, const Sphere* scene,
                          int sphereCount) {
    Vec3 lightPos(4, 6, 2);
    float tHit;
    int hit = closestHit(origin, dir, tHit, scene, sphereCount);

    if (hit < 0) {
        float t = 0.5f * (dir.y + 1.0f);
        return Vec3(1, 1, 1) * (1 - t) + Vec3(0.4f, 0.6f, 1.0f) * t;
    }

    Vec3 p = origin + dir * tHit;
    Vec3 n = normalize(p - scene[hit].center);
    Vec3 base = scene[hit].color;

    if (scene[hit].isFloor) {
        int cx = (int)floorf(p.x), cz = (int)floorf(p.z);
        bool white = ((cx + cz) & 1) == 0;
        base = white ? Vec3(0.9f, 0.9f, 0.9f) : Vec3(0.2f, 0.2f, 0.2f);
    }

    Vec3 toLight = normalize(lightPos - p);
    float diffuse = fmaxf(0.0f, dot(n, toLight));
    float shadowT;
    int blocked = closestHit(p + n * 0.001f, toLight, shadowT, scene, sphereCount);
    float shade = (blocked >= 0) ? 0.3f : 1.0f;
    float brightness = 0.15f + diffuse * shade;
    return base * brightness;
}

__global__ void render(unsigned char* image, int W, int H, const Sphere* scene,
                       int sphereCount) {
    int px = blockIdx.x * blockDim.x + threadIdx.x;
    int py = blockIdx.y * blockDim.y + threadIdx.y;
    if (px >= W || py >= H) return;

    Vec3 cameraPos(0, 1, 3);
    float aspect = (float)W / H;
    float u = (2.0f * (px + 0.5f) / W - 1.0f) * aspect;
    float v = (1.0f - 2.0f * (py + 0.5f) / H);
    Vec3 dir = normalize(Vec3(u, v, -1.5f));
    Vec3 color = rayColor(cameraPos, dir, scene, sphereCount);

    size_t idx = ((size_t)py * W + px) * 3;
    image[idx + 0] = (unsigned char)(fminf(color.x, 1.0f) * 255);
    image[idx + 1] = (unsigned char)(fminf(color.y, 1.0f) * 255);
    image[idx + 2] = (unsigned char)(fminf(color.z, 1.0f) * 255);
}

int main() {
    const int W = 800, H = 600;
    Sphere scene[] = {
        { Vec3(0, -1000.5f, -1), 1000.0f, Vec3(0.5f, 0.5f, 0.5f), true },
        { Vec3(0, 0.0f, -1), 0.5f, Vec3(0.9f, 0.2f, 0.2f), false },
        { Vec3(-1.1f, 0.0f, -1), 0.5f, Vec3(0.2f, 0.7f, 0.3f), false },
        { Vec3(1.1f, 0.0f, -1), 0.5f, Vec3(0.2f, 0.4f, 0.9f), false }
    };
    int sphereCount = sizeof(scene) / sizeof(scene[0]);
    size_t imageBytes = (size_t)W * H * 3;
    std::vector<unsigned char> image(imageBytes);

    Sphere* d_scene;
    unsigned char* d_image;
    cudaMalloc((void**)&d_scene, sizeof(scene));
    cudaMalloc((void**)&d_image, imageBytes);
    cudaMemcpy(d_scene, scene, sizeof(scene), cudaMemcpyHostToDevice);

    dim3 block(16, 16);
    dim3 grid((W - 1) / block.x + 1, (H - 1) / block.y + 1);

    render<<<grid, block>>>(d_image, W, H, d_scene, sphereCount);
    cudaError_t status = cudaGetLastError();
    if (status == cudaSuccess) status = cudaDeviceSynchronize();
    if (status != cudaSuccess) {
        fprintf(stderr, "CUDA error: %s\n", cudaGetErrorString(status));
        return 1;
    }

    cudaEvent_t start, stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);
    const int runs = 10;
    cudaEventRecord(start);
    for (int i = 0; i < runs; i++) {
        render<<<grid, block>>>(d_image, W, H, d_scene, sphereCount);
    }
    cudaEventRecord(stop);
    cudaEventSynchronize(stop);
    float milliseconds;
    cudaEventElapsedTime(&milliseconds, start, stop);
    cudaMemcpy(image.data(), d_image, imageBytes, cudaMemcpyDeviceToHost);
    cudaEventDestroy(start);
    cudaEventDestroy(stop);
    cudaFree(d_image);
    cudaFree(d_scene);

    FILE* f = fopen("image.ppm", "wb");
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    fwrite(image.data(), 1, image.size(), f);
    fclose(f);

    printf("Wrote image.ppm (%dx%d)\n", W, H);
    printf("Kernel time: %.4f ms (average of %d runs)\n", milliseconds / runs, runs);
    return 0;
}
