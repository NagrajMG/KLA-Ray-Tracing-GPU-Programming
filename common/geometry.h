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

struct QuarticRoots {
    float r0;
    float r1;
    float r2;
    float r3;
};

HD void sort_pair(float &a, float &b) {
    if (b < a) {
        float temporary = a;
        a = b;
        b = temporary;
    }
}

HD int solve_quartic(float a4, float a3, float a2, float a1, float a0, QuarticRoots &roots) {
    // empty slots go to end after sorting
    roots = {FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX};
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
            if (i == 0) {
                roots.r0 = root - a / 4.0f;
                if (root != 0.0f)
                    roots.r1 = -root - a / 4.0f;
            } else {
                roots.r2 = root - a / 4.0f;
                if (root != 0.0f)
                    roots.r3 = -root - a / 4.0f;
            }
            count += root == 0.0f ? 1 : 2;
        }
    } else {
        float cubic_a = 2.0f * p;
        float cubic_b = p * p - 4.0f * r;
        float cubic_c = -q * q;
        float z = fmaxf(0.0f, largest_cubic_root(cubic_a, cubic_b, cubic_c));
        float bound = 1.0f + fmaxf(fabsf(cubic_a), fmaxf(fabsf(cubic_b), fabsf(cubic_c)));
        float value = ((z + cubic_a) * z + cubic_b) * z + cubic_c;
        while (true) {
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
            value = residual;
        }
        if (z == 0.0f)
            return 0;
        float s = sqrtf(z);
        int first = solve_quadratic(1.0f, s, (p + z - q / s) / 2.0f, pair);
        if (first > 0)
            roots.r0 = pair[0] - a / 4.0f;
        if (first > 1)
            roots.r1 = pair[1] - a / 4.0f;
        int second = solve_quadratic(1.0f, -s, (p + z + q / s) / 2.0f, pair);
        if (second > 0)
            roots.r2 = pair[0] - a / 4.0f;
        if (second > 1)
            roots.r3 = pair[1] - a / 4.0f;
        count = first + second;
    }
    if (count == 0)
        return 0;
    sort_pair(roots.r0, roots.r1);
    sort_pair(roots.r2, roots.r3);
    sort_pair(roots.r0, roots.r2);
    sort_pair(roots.r1, roots.r3);
    sort_pair(roots.r1, roots.r2);
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
    float radius = torus.radius;
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

    float major = torus.scaled_major_radius;
    float minor = torus.scaled_minor_radius;
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
    QuarticRoots roots;
    int count = solve_quartic(a4, a3, a2, a1, a0, roots);
    for (int i = 0; i < count; ++i) {
        float root = i == 0 ? roots.r0 : i == 1 ? roots.r1 : i == 2 ? roots.r2 : roots.r3;
        float value = (((a4 * root + a3) * root + a2) * root + a1) * root + a0;
        while (true) {
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
            value = residual;
        }
        float t = shift + root * parameter_scale;
        if (t > t_min && t < t_max)
            return t;
    }
    return -1.0f;
}

HD int intersect_tori(const Ray &ray, const Scene &scene, float t_min, float &t_max) {
    // each torus inside own cell, first hit is nearest
    float half_x = 0.5f * TorusColumns * TorusSpacing;
    float half_z = 0.5f * TorusRows * TorusSpacing;
    float enter = t_min;
    float exit = t_max;
    for (int axis = 0; axis < 2; ++axis) {
        float origin = axis == 0 ? ray.origin.x : ray.origin.z;
        float direction = axis == 0 ? ray.direction.x : ray.direction.z;
        float half = axis == 0 ? half_x : half_z;
        if (direction == 0.0f) {
            if (fabsf(origin) > half)
                return -1;
        } else {
            float first = (-half - origin) / direction;
            float last = (half - origin) / direction;
            enter = fmaxf(enter, fminf(first, last));
            exit = fminf(exit, fmaxf(first, last));
        }
    }
    if (enter > exit)
        return -1;

    Vec3 entry = ray.origin + ray.direction * enter;
    int column =
        (int)fminf(TorusColumns - 1, fmaxf(0.0f, floorf((entry.x + half_x) / TorusSpacing)));
    int row = (int)fminf(TorusRows - 1, fmaxf(0.0f, floorf((entry.z + half_z) / TorusSpacing)));
    int step_x = ray.direction.x < 0.0f ? -1 : 1;
    int step_z = ray.direction.z < 0.0f ? -1 : 1;
    float delta_x = ray.direction.x == 0.0f ? FLT_MAX : TorusSpacing / fabsf(ray.direction.x);
    float delta_z = ray.direction.z == 0.0f ? FLT_MAX : TorusSpacing / fabsf(ray.direction.z);
    float next_x =
        ray.direction.x == 0.0f
            ? FLT_MAX
            : ((column + (step_x > 0)) * TorusSpacing - half_x - ray.origin.x) / ray.direction.x;
    float next_z =
        ray.direction.z == 0.0f
            ? FLT_MAX
            : ((row + (step_z > 0)) * TorusSpacing - half_z - ray.origin.z) / ray.direction.z;
    while (column >= 0 && column < TorusColumns && row >= 0 && row < TorusRows) {
        int index = row * TorusColumns + column;
        float t = intersect_torus(ray, scene.tori[index], t_min, t_max);
        if (t >= 0.0f) {
            t_max = t;
            return index;
        }
        float boundary = fminf(next_x, next_z);
        if (boundary >= exit)
            break;
        // at a corner, move in both directions
        if (next_x == boundary) {
            column += step_x;
            next_x += delta_x;
        }
        if (next_z == boundary) {
            row += step_z;
            next_z += delta_z;
        }
    }
    return -1;
}

HD float intersect_rectangle(const Ray &ray, const Rectangle &rectangle, float t_min, float t_max,
                             float &u, float &v) {
    Vec3 normal = rectangle.normal;
    float denominator = dot(ray.direction, normal);
    if (denominator == 0.0f)
        return -1.0f;
    float t = dot(rectangle.corner - ray.origin, normal) / denominator;
    if (t <= t_min || t >= t_max)
        return -1.0f;
    Vec3 point = ray.origin + ray.direction * t - rectangle.corner;
    u = dot(point, rectangle.edge_u) * rectangle.inverse_u_squared;
    v = dot(point, rectangle.edge_v) * rectangle.inverse_v_squared;
    return u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f ? t : -1.0f;
}

HD bool occluded(const Ray &ray, const Scene &scene, float t_min, float t_max) {
    float u, v;
    for (int i = 0; i < RectangleCount; ++i) {
        if (intersect_rectangle(ray, scene.rectangles[i], t_min, t_max, u, v) >= 0.0f)
            return true;
    }
    for (int i = 0; i < LightCount; ++i) {
        if (intersect_rectangle(ray, scene.lights[i].surface, t_min, t_max, u, v) >= 0.0f)
            return true;
    }
    return intersect_tori(ray, scene, t_min, t_max) >= 0;
}

HD Hit intersect_scene(const Ray &ray, const Scene &scene, float t_min, float t_max) {
    Hit hit{};
    hit.t = t_max;
    hit.material_index = -1;
    hit.light_index = -1;
    int torus_index = intersect_tori(ray, scene, t_min, hit.t);
    int rectangle_index = -1;
    if (torus_index >= 0)
        hit.material_index = scene.tori[torus_index].material_index;
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
        hit.normal = normalize(rectangle.normal);
    }
    if (dot(ray.direction, hit.normal) > 0.0f) {
        hit.normal = hit.normal * -1.0f;
    }
    return hit;
}
