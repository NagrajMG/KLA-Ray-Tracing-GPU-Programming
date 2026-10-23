#include <cuda_runtime.h>

#include <cmath>
#include <cstdio>
#include <vector>

struct Vec3 {
    double x, y, z;
};

__host__ __device__ Vec3 operator+(Vec3 a, Vec3 b) { 
    return {a.x + b.x, a.y + b.y, a.z + b.z}; 
}

__host__ __device__ Vec3 operator-(Vec3 a, Vec3 b) { 
    return {a.x - b.x, a.y - b.y, a.z - b.z}; 
}

__host__ __device__ Vec3 operator*(Vec3 a, double s) { 
    return {a.x * s, a.y * s, a.z * s}; 
}

__host__ __device__ Vec3 operator/(Vec3 a, double s) { 
    return a * (1.0 / s); 
}

__host__ __device__ double dot(Vec3 a, Vec3 b) { 
    return a.x * b.x + a.y * b.y + a.z * b.z; 
}

__host__ __device__ double length(Vec3 a) { 
    return sqrt(dot(a, a)); 
}

__host__ __device__ Vec3 normalize(Vec3 a) { 
    return a / length(a); 
}

__host__ __device__ Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

__host__ __device__ Vec3 reflect(Vec3 direction, Vec3 normal) {
    return direction - normal * (2.0 * dot(direction, normal));
}

struct Ray {
    Vec3 origin, direction;
};

struct Config {
    int width, height, samples, max_reflections;
    Vec3 look_from, look_at, up;
    double vfov;
    Vec3 torus_center;
    double major_radius, minor_radius, floor_y;
    Vec3 light;
};

Config configuration() {
    return {640, 360, 4, 2,
            {3.8, 3.0, 5.2}, {0.0, 0.8, 0.0}, {0.0, 1.0, 0.0}, 42.0,
            {0.0, 1.0, 0.0}, 1.0, 0.35, 0.0, {-3.0, 6.0, 4.0}};
}

struct Camera {
    Vec3 origin, u, v, w, pixel00, pixel_du, pixel_dv;
    double aspect;
};

Camera make_camera(const Config& config) {
    Camera camera{};
    camera.origin = config.look_from;
    camera.w = normalize(config.look_from - config.look_at);
    camera.u = normalize(cross(config.up, camera.w));
    camera.v = cross(camera.w, camera.u);
    camera.aspect = (double)config.width / config.height;
    double focus_distance = length(config.look_from - config.look_at);
    double theta = config.vfov * 3.14159265358979323846 / 180.0;
    double viewport_height = 2.0 * tan(theta * 0.5) * focus_distance;
    Vec3 horizontal = camera.u * (viewport_height * camera.aspect);
    Vec3 downward = camera.v * -viewport_height;
    camera.pixel_du = horizontal / config.width;
    camera.pixel_dv = downward / config.height;
    camera.pixel00 = camera.origin - camera.w * focus_distance - (horizontal + downward) * 0.5
                     + (camera.pixel_du + camera.pixel_dv) * 0.5;
    return camera;
}

__device__ Ray ray_for_sample(const Camera& camera, const Config& config, int pixel, int sample) {
    double offset_x = (sample % 2) * 0.5 - 0.25;
    double offset_y = (sample / 2) * 0.5 - 0.25;
    double x = pixel % config.width + offset_x;
    double y = pixel / config.width + offset_y;
    Vec3 target = camera.pixel00 + camera.pixel_du * x + camera.pixel_dv * y;
    return {camera.origin, normalize(target - camera.origin)};
}

__device__ double polynomial(const double* coefficients, int degree, double x) {
    double value = coefficients[degree];
    for (int i = degree - 1; i >= 0; --i) value = value * x + coefficients[i];
    return value;
}

__device__ double polynomial_tolerance(const double* coefficients, int degree) {
    double scale = 0.0;
    for (int i = 0; i <= degree; ++i) scale += fabs(coefficients[i]);
    return 2.2204460492503131e-16 * scale;
}

__device__ double bisect_root(const double* coefficients, int degree, double left, double right) {
    double f_left = polynomial(coefficients, degree, left);
    while (true) {
        double middle = left + (right - left) * 0.5;
        if (middle == left || middle == right) break;
        double f_middle = polynomial(coefficients, degree, middle);
        if (f_middle == 0.0) return middle;
        if ((f_left < 0.0) != (f_middle < 0.0)) {
            right = middle;
        } else {
            left = middle;
            f_left = f_middle;
        }
    }
    return left + (right - left) * 0.5;
}

__device__ int quadratic_roots(double a, double b, double c, double roots[2]) {
    double discriminant = b * b - 4.0 * a * c;
    double tolerance = 2.2204460492503131e-16 * (b * b + fabs(4.0 * a * c));
    if (discriminant < -tolerance) return 0;
    if (fabs(discriminant) <= tolerance) {
        roots[0] = -b / (2.0 * a);
        return 1;
    }
    double q = -0.5 * (b + copysign(sqrt(discriminant), b));
    roots[0] = q / a;
    roots[1] = c / q;
    if (roots[1] < roots[0]) {
        double temporary = roots[0];
        roots[0] = roots[1];
        roots[1] = temporary;
    }
    return 2;
}

__device__ int interval_roots(const double* coefficients, int degree, double lower, double upper,
                             const double* stationary, int stationary_count, double* roots) {
    double cuts[5];
    int cut_count = 1;
    cuts[0] = lower;
    for (int i = 0; i < stationary_count; ++i) {
        if (stationary[i] > lower && stationary[i] < upper) cuts[cut_count++] = stationary[i];
    }
    cuts[cut_count++] = upper;
    double tolerance = polynomial_tolerance(coefficients, degree);
    int count = 0;
    for (int i = 0; i < cut_count; ++i) {
        double value = polynomial(coefficients, degree, cuts[i]);
        if (fabs(value) <= tolerance && count < degree) roots[count++] = cuts[i];
        if (i + 1 == cut_count || fabs(value) <= tolerance) continue;
        double next = polynomial(coefficients, degree, cuts[i + 1]);
        if (fabs(next) > tolerance && (value < 0.0) != (next < 0.0) && count < degree) {
            roots[count++] = bisect_root(coefficients, degree, cuts[i], cuts[i + 1]);
        }
    }
    return count;
}

__device__ int quartic_roots(const double coefficients[5], double lower, double upper,
                            double roots[4]) {
    double derivative[4] = {coefficients[1], 2.0 * coefficients[2],
                            3.0 * coefficients[3], 4.0 * coefficients[4]};
    double turning_points[2];
    int turning_count = quadratic_roots(3.0 * derivative[3], 2.0 * derivative[2],
                                        derivative[1], turning_points);
    double stationary[3];
    int stationary_count = interval_roots(derivative, 3, lower, upper,
                                          turning_points, turning_count, stationary);
    return interval_roots(coefficients, 4, lower, upper, stationary, stationary_count, roots);
}

__device__ double intersect_torus(const Ray& ray, const Config& config, double t_min, double t_max) {
    Vec3 relative = ray.origin - config.torus_center;
    double direction_squared = dot(ray.direction, ray.direction);
    double direction_length = sqrt(direction_squared);
    double radius = config.major_radius + config.minor_radius;
    double shift = -dot(relative, ray.direction) / direction_squared;
    double parameter_scale = radius / direction_length;
    Vec3 origin = (relative + ray.direction * shift) / radius;
    Vec3 direction = ray.direction / direction_length;
    double span_squared = 1.0 - dot(origin, origin);
    if (span_squared < -2.2204460492503131e-16) return -1.0;
    double span = sqrt(fmax(0.0, span_squared));
    double lower = fmax(-span, (t_min - shift) / parameter_scale);
    double upper = fmin(span, (t_max - shift) / parameter_scale);
    if (lower > upper) return -1.0;

    double major = config.major_radius / radius;
    double minor = config.minor_radius / radius;
    double g2 = dot(direction, direction);
    double g1 = 2.0 * dot(origin, direction);
    double g0 = dot(origin, origin) + major * major - minor * minor;
    double k = 4.0 * major * major;
    double coefficients[5] = {
        g0 * g0 - k * (origin.x * origin.x + origin.z * origin.z),
        2.0 * g1 * g0 - 2.0 * k * (origin.x * direction.x + origin.z * direction.z),
        g1 * g1 + 2.0 * g2 * g0 - k * (direction.x * direction.x + direction.z * direction.z),
        2.0 * g2 * g1,
        g2 * g2
    };
    double roots[4];
    int count = quartic_roots(coefficients, lower, upper, roots);
    for (int i = 0; i < count; ++i) {
        double t = shift + roots[i] * parameter_scale;
        if (t > t_min && t < t_max) return t;
    }
    return -1.0;
}

struct Hit {
    int object;
    double t;
    Vec3 normal;
};

__device__ Hit intersect_scene(const Ray& ray, const Config& config, double t_min, double t_max) {
    // 0 = no hit, 1 = torus, 2 = floor
    Hit hit{0, t_max, {0.0, 0.0, 0.0}};

    // Ray is parallel to the floor
    if (ray.direction.y != 0.0) {
        double t = (config.floor_y - ray.origin.y) / ray.direction.y;
        if (t > t_min && t < hit.t) hit = {2, t, {0.0, 1.0, 0.0}};
    }

    double torus_t = intersect_torus(ray, config, t_min, hit.t);
    if (torus_t > t_min) {
        Vec3 q = ray.origin + ray.direction * torus_t - config.torus_center;
        double r2 = config.major_radius * config.major_radius;
        double s = dot(q, q) + r2 - config.minor_radius * config.minor_radius;
        Vec3 normal = {q.x * (s - 2.0 * r2), q.y * s, q.z * (s - 2.0 * r2)};
        hit = {1, torus_t, normalize(normal)};
    }

    if (dot(ray.direction, hit.normal) > 0.0) hit.normal = hit.normal * -1.0;
    return hit;
}

__device__ Vec3 sky(Vec3 direction) {
    double blend = 0.5 * (direction.y + 1.0);
    return Vec3{1.0, 1.0, 1.0} * (1.0 - blend) + Vec3{0.4, 0.6, 1.0} * blend;
}

__device__ Vec3 trace(Ray ray, const Config& config) {
    Vec3 colour{0.0, 0.0, 0.0};
    double throughput = 1.0;
    for (int bounce = 0; bounce <= config.max_reflections; bounce++) {
        Hit hit = intersect_scene(ray, config, 1e-6, 1e30);
        if (hit.object == 0) {
            colour = colour + sky(ray.direction) * throughput;
            break;
        }
        Vec3 point = ray.origin + ray.direction * hit.t;
        Vec3 base{0.85, 0.28, 0.09};
        double reflectivity = 0.3;
        if (hit.object == 2) {
            bool white = fmod(floor(point.x) + floor(point.z), 2.0) == 0.0;
            base = white ? Vec3{0.85, 0.85, 0.85} : Vec3{0.12, 0.12, 0.12};
            reflectivity = 0.18;
        }
        Vec3 origin = point + hit.normal * 1e-5;
        Vec3 to_light = config.light - origin;
        double light_distance = length(to_light);
        Vec3 light_direction = to_light / light_distance;
        double diffuse = fmax(0.0, dot(hit.normal, light_direction));
        bool blocked = false;
        if (diffuse > 0.0) {
            blocked = intersect_scene({origin, light_direction}, config, 1e-6, light_distance).object != 0;
        }
        Vec3 local = base * (0.12 + (blocked ? 0.0 : 0.88 * diffuse));
        colour = colour + local * (throughput * (1.0 - reflectivity));
        if (bounce == config.max_reflections) break;
        throughput *= reflectivity;
        ray = {origin, normalize(reflect(ray.direction, hit.normal))};
    }
    return colour;
}

__device__ Vec3 render_pixel(const Config& config, const Camera& camera, int pixel) {
    Vec3 colour{0.0, 0.0, 0.0};
    for (int sample = 0; sample < config.samples; sample++) {
        colour = colour + trace(ray_for_sample(camera, config, pixel, sample), config);
    }
    return colour / config.samples;
}

unsigned char display_channel(double value) {
    return (unsigned char)(255.999 * sqrt(fmax(0.0, fmin(1.0, value))));
}

void write_image(const char* filename, const Config& config, const std::vector<Vec3>& image) {
    std::vector<unsigned char> pixels(image.size() * 3);
    for (size_t i = 0; i < image.size(); i++) {
        pixels[3 * i] = display_channel(image[i].x);
        pixels[3 * i + 1] = display_channel(image[i].y);
        pixels[3 * i + 2] = display_channel(image[i].z);
    }
    FILE* file = fopen(filename, "wb");
    fprintf(file, "P6\n%d %d\n255\n", config.width, config.height);
    fwrite(pixels.data(), 1, pixels.size(), file);
    fclose(file);
}

__global__ void render(Vec3* image, Config config, Camera camera) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= config.width || y >= config.height) return;
    int pixel = y * config.width + x;
    image[pixel] = render_pixel(config, camera, pixel);
}

int main() {
    Config config = configuration();
    Camera camera = make_camera(config);
    std::vector<Vec3> image(config.width * config.height);

    size_t bytes = image.size() * sizeof(Vec3);
    Vec3* device_image;
    cudaMalloc(&device_image, bytes);

    dim3 block(16, 16);
    dim3 grid((config.width + block.x - 1) / block.x, (config.height + block.y - 1) / block.y);

    // warming up
    render<<<grid, block>>>(device_image, config, camera);
    cudaDeviceSynchronize();

    cudaEvent_t start, stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);
    cudaEventRecord(start);
    for (int run = 0; run < 20; run++) {
        render<<<grid, block>>>(device_image, config, camera);
    }
    cudaEventRecord(stop);
    cudaEventSynchronize(stop);

    float milliseconds;
    cudaEventElapsedTime(&milliseconds, start, stop);
    printf("GPU kernel time: %.3f ms\n", milliseconds / 20, 20);
    cudaEventDestroy(start);
    cudaEventDestroy(stop);

    cudaMemcpy(image.data(), device_image, bytes, cudaMemcpyDeviceToHost);
    cudaFree(device_image);

    write_image("gpu.ppm", config, image);
    return 0;
}
