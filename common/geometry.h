#pragma once

#include "types.h"

#include <cfloat>

HD int solve_quadratic(float a, float b, float c, float roots[2]) {
    float discriminant = b * b - 4.0f * a * c;
    float tolerance = FLT_EPSILON * (b * b + fabsf(4.0f * a * c));
    if (discriminant < -tolerance)
        return 0;
    if (fabsf(discriminant) <= tolerance) {
        roots[0] = -b / (2.0f * a);
        return 1;
    }
    float q = -0.5f * (b + copysignf(sqrtf(discriminant), b));
    roots[0] = q / a;
    roots[1] = c / q;
    return 2;
}

HD float largest_cubic_root(float a, float b, float c) {
    float p = b - a * a / 3.0f;
    float q = 2.0f * a * a * a / 27.0f - a * b / 3.0f + c;
    float third_p = p / 3.0f;
    float half_q = q / 2.0f;
    float discriminant = half_q * half_q + third_p * third_p * third_p;
    if (discriminant >= 0.0f) {
        float u = cbrtf(-half_q - copysignf(sqrtf(discriminant), half_q));
        float v = u == 0.0f ? 0.0f : -third_p / u;
        float root = u + v;
        if (discriminant == 0.0f)
            root = fmaxf(root, -u);
        return root - a / 3.0f;
    }
    float radius = sqrtf(-third_p);
    float cosine = -half_q / (radius * radius * radius);
    float angle = acosf(fmaxf(-1.0f, fminf(1.0f, cosine))) / 3.0f;
    return 2.0f * radius * cosf(angle) - a / 3.0f;
}

HD int solve_quartic(float a4, float a3, float a2, float a1, float a0, float roots[4]) {
    float a = a3 / a4;
    float b = a2 / a4;
    float c = a1 / a4;
    float d = a0 / a4;
    float p = b - 3.0f * a * a / 8.0f;
    float q = c - a * b / 2.0f + a * a * a / 8.0f;
    float r = d - a * c / 4.0f + a * a * b / 16.0f - 3.0f * a * a * a * a / 256.0f;
    int count = 0;
    float pair[2];
    if (q == 0.0f) {
        int squares = solve_quadratic(1.0f, p, r, pair);
        for (int i = 0; i < squares; ++i) {
            if (pair[i] < 0.0f)
                continue;
            float root = sqrtf(pair[i]);
            roots[count++] = root - a / 4.0f;
            if (root != 0.0f)
                roots[count++] = -root - a / 4.0f;
        }
    } else {
        float cubic_a = 2.0f * p;
        float cubic_b = p * p - 4.0f * r;
        float cubic_c = -q * q;
        float z = fmaxf(0.0f, largest_cubic_root(cubic_a, cubic_b, cubic_c));
        float bound = 1.0f + fmaxf(fabsf(cubic_a), fmaxf(fabsf(cubic_b), fabsf(cubic_c)));
        while (true) {
            float value = ((z + cubic_a) * z + cubic_b) * z + cubic_c;
            float slope = (3.0f * z + 2.0f * cubic_a) * z + cubic_b;
            if (slope == 0.0f)
                break;
            float next = z - value / slope;
            if (next == z || next < 0.0f || next > bound || !std::isfinite(next))
                break;
            float residual = ((next + cubic_a) * next + cubic_b) * next + cubic_c;
            if (fabsf(residual) >= fabsf(value))
                break;
            z = next;
        }
        if (z == 0.0f)
            return 0;
        float s = sqrtf(z);
        int first = solve_quadratic(1.0f, s, (p + z - q / s) / 2.0f, pair);
        for (int i = 0; i < first; ++i)
            roots[count++] = pair[i] - a / 4.0f;
        int second = solve_quadratic(1.0f, -s, (p + z + q / s) / 2.0f, pair);
        for (int i = 0; i < second; ++i)
            roots[count++] = pair[i] - a / 4.0f;
    }
    for (int i = 1; i < count; ++i) {
        float root = roots[i];
        int j = i;
        while (j > 0 && root < roots[j - 1]) {
            roots[j] = roots[j - 1];
            --j;
        }
        roots[j] = root;
    }
    return count;
}

HD Vec3 torus_local(Vec3 vector, TorusOrientation orientation) {
    return orientation == Flat ? vector : Vec3{vector.x, vector.z, -vector.y};
}

HD Vec3 torus_world(Vec3 vector, TorusOrientation orientation) {
    return orientation == Flat ? vector : Vec3{vector.x, -vector.z, vector.y};
}

HD float intersect_torus(const Ray &ray, const Torus &torus, float t_min, float t_max) {
    Vec3 relative = torus_local(ray.origin - torus.center, torus.orientation);
    Vec3 ray_direction = torus_local(ray.direction, torus.orientation);
    float direction_squared = dot(ray_direction, ray_direction);
    float direction_length = sqrtf(direction_squared);
    float radius = torus.major_radius + torus.minor_radius;
    float shift = -dot(relative, ray_direction) / direction_squared;
    float parameter_scale = radius / direction_length;
    Vec3 origin = (relative + ray_direction * shift) / radius;
    Vec3 direction = ray_direction / direction_length;
    float distance_squared = dot(origin, origin);
    float span_squared = 1.0f - distance_squared;
    if (span_squared < -FLT_EPSILON * (1.0f + distance_squared))
        return -1.0f;
    float span = sqrtf(fmaxf(0.0f, span_squared));
    float lower = fmaxf(-span, (t_min - shift) / parameter_scale);
    float upper = fminf(span, (t_max - shift) / parameter_scale);
    if (lower > upper)
        return -1.0f;

    float major = torus.major_radius / radius;
    float minor = torus.minor_radius / radius;
    float g2 = dot(direction, direction);
    float g1 = 2.0f * dot(origin, direction);
    float rho = sqrtf(origin.x * origin.x + origin.z * origin.z);
    float vertical = (origin.y - minor) * (origin.y + minor);
    float radial = (rho - major) * (rho + major) + vertical;
    float k = 4.0f * major * major;
    float a4 = g2 * g2;
    float a3 = 2.0f * g2 * g1;
    float a2 = g1 * g1 + 2.0f * g2 * radial + k * direction.y * direction.y;
    float a1 = 2.0f * g1 * radial + 2.0f * k * origin.y * direction.y;
    float a0 =
        ((rho - major) * (rho - major) + vertical) * ((rho + major) * (rho + major) + vertical);
    float roots[4];
    int count = solve_quartic(a4, a3, a2, a1, a0, roots);
    for (int i = 0; i < count; ++i) {
        float root = roots[i];
        while (true) {
            float value = (((a4 * root + a3) * root + a2) * root + a1) * root + a0;
            float slope = ((4.0f * a4 * root + 3.0f * a3) * root + 2.0f * a2) * root + a1;
            if (slope == 0.0f)
                break;
            float next = root - value / slope;
            if (next == root || next < lower || next > upper || !std::isfinite(next))
                break;
            float residual = (((a4 * next + a3) * next + a2) * next + a1) * next + a0;
            if (fabsf(residual) >= fabsf(value))
                break;
            root = next;
        }
        float t = shift + root * parameter_scale;
        if (t > t_min && t < t_max)
            return t;
    }
    return -1.0f;
}

HD float intersect_rectangle(const Ray &ray, const Rectangle &rectangle, float t_min, float t_max,
                             float &u, float &v) {
    Vec3 normal = cross(rectangle.edge_u, rectangle.edge_v);
    float denominator = dot(ray.direction, normal);
    if (denominator == 0.0f)
        return -1.0f;
    float t = dot(rectangle.corner - ray.origin, normal) / denominator;
    if (t <= t_min || t >= t_max)
        return -1.0f;
    Vec3 point = ray.origin + ray.direction * t - rectangle.corner;
    u = dot(point, rectangle.edge_u) / dot(rectangle.edge_u, rectangle.edge_u);
    v = dot(point, rectangle.edge_v) / dot(rectangle.edge_v, rectangle.edge_v);
    return u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f ? t : -1.0f;
}

HD Hit intersect_scene(const Ray &ray, const Scene &scene, float t_min, float t_max) {
    Hit hit{};
    hit.t = t_max;
    hit.material_index = -1;
    hit.light_index = -1;
    int torus_index = -1;
    int rectangle_index = -1;
    for (int i = 0; i < TorusCount; ++i) {
        float t = intersect_torus(ray, scene.tori[i], t_min, hit.t);
        if (t < 0.0f)
            continue;
        hit.t = t;
        hit.material_index = scene.tori[i].material_index;
        torus_index = i;
    }
    for (int i = 0; i < RectangleCount; ++i) {
        float u, v;
        float t = intersect_rectangle(ray, scene.rectangles[i], t_min, hit.t, u, v);
        if (t < 0.0f)
            continue;
        hit.t = t;
        hit.u = u;
        hit.v = v;
        hit.material_index = scene.rectangles[i].material_index;
        rectangle_index = i;
        torus_index = -1;
    }
    for (int i = 0; i < LightCount; ++i) {
        float u, v;
        float t = intersect_rectangle(ray, scene.lights[i].surface, t_min, hit.t, u, v);
        if (t < 0.0f)
            continue;
        hit.t = t;
        hit.material_index = -1;
        hit.light_index = i;
    }
    if (hit.material_index < 0)
        return hit;
    hit.point = ray.origin + ray.direction * hit.t;
    if (torus_index >= 0) {
        const Torus &torus = scene.tori[torus_index];
        Vec3 point = torus_local(hit.point - torus.center, torus.orientation);
        float rho = sqrtf(point.x * point.x + point.z * point.z);
        float radial = rho - torus.major_radius;
        Vec3 normal = normalize(Vec3{point.x * radial / rho, point.y, point.z * radial / rho});
        hit.normal = torus_world(normal, torus.orientation);
    } else {
        const Rectangle &rectangle = scene.rectangles[rectangle_index];
        hit.normal = normalize(cross(rectangle.edge_u, rectangle.edge_v));
    }
    if (dot(ray.direction, hit.normal) > 0.0f) {
        hit.normal = hit.normal * -1.0f;
    }
    return hit;
}
