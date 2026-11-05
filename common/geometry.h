#pragma once

#include "types.h"

#include <cfloat>

HD int solve_quadratic(Real a, Real b, Real c, Real roots[2]) {
    Real discriminant = b * b - 4.0f * a * c;
    Real tolerance = RealEpsilon * (b * b + real_abs(4.0f * a * c));
    if (discriminant < -tolerance)
        return 0;
    if (real_abs(discriminant) <= tolerance) {
        roots[0] = -b / (2.0f * a);
        return 1;
    }
    Real q = -0.5f * (b + real_copysign(real_sqrt(discriminant), b));
    roots[0] = q / a;
    roots[1] = c / q;
    return 2;
}

HD Real largest_cubic_root(Real a, Real b, Real c) {
    Real p = b - a * a / 3.0f;
    Real q = 2.0f * a * a * a / 27.0f - a * b / 3.0f + c;
    Real third_p = p / 3.0f;
    Real half_q = q / 2.0f;
    Real discriminant = half_q * half_q + third_p * third_p * third_p;
    if (discriminant >= 0.0f) {
        Real u = real_cbrt(-half_q - real_copysign(real_sqrt(discriminant), half_q));
        Real v = u == 0.0f ? Real(0.0f) : -third_p / u;
        Real root = u + v;
        if (discriminant == 0.0f)
            root = real_max(root, -u);
        return root - a / 3.0f;
    }
    Real radius = real_sqrt(-third_p);
    Real cosine = -half_q / (radius * radius * radius);
    Real angle = real_acos(real_max(-1.0f, real_min(1.0f, cosine))) / 3.0f;
    return 2.0f * radius * real_cos(angle) - a / 3.0f;
}

struct QuarticRoots {
    Real r0;
    Real r1;
    Real r2;
    Real r3;
};

HD void sort_pair(Real &a, Real &b) {
    if (b < a) {
        Real temporary = a;
        a = b;
        b = temporary;
    }
}

HD int solve_quartic(Real a4, Real a3, Real a2, Real a1, Real a0, QuarticRoots &roots) {
    // empty slots go to end after sorting
    roots = {RealMax, RealMax, RealMax, RealMax};
    Real a = a3 / a4;
    Real b = a2 / a4;
    Real c = a1 / a4;
    Real d = a0 / a4;
    Real p = b - 3.0f * a * a / 8.0f;
    Real q = c - a * b / 2.0f + a * a * a / 8.0f;
    Real r = d - a * c / 4.0f + a * a * b / 16.0f - 3.0f * a * a * a * a / 256.0f;
    int count = 0;
    Real pair[2];
    if (q == 0.0f) {
        int squares = solve_quadratic(1.0f, p, r, pair);
        for (int i = 0; i < squares; ++i) {
            if (pair[i] < 0.0f)
                continue;
            Real root = real_sqrt(pair[i]);
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
        Real cubic_a = 2.0f * p;
        Real cubic_b = p * p - 4.0f * r;
        Real cubic_c = -q * q;
        Real z = real_max(0.0f, largest_cubic_root(cubic_a, cubic_b, cubic_c));
        Real bound = 1.0f + real_max(real_abs(cubic_a), real_max(real_abs(cubic_b), real_abs(cubic_c)));
        Real value = ((z + cubic_a) * z + cubic_b) * z + cubic_c;
        while (true) {
            Real slope = (3.0f * z + 2.0f * cubic_a) * z + cubic_b;
            if (slope == 0.0f)
                break;
            Real next = z - value / slope;
            if (next == z || next < 0.0f || next > bound || !std::isfinite(float(next)))
                break;
            Real residual = ((next + cubic_a) * next + cubic_b) * next + cubic_c;
            if (real_abs(residual) >= real_abs(value))
                break;
            z = next;
            value = residual;
        }
        if (z == 0.0f)
            return 0;
        Real s = real_sqrt(z);
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

HD Real intersect_torus(const Ray &ray, const Torus &torus, Real t_min, Real t_max) {
    Vec3 relative = torus_local(ray.origin - torus.center, torus.orientation);
    Vec3 ray_direction = torus_local(ray.direction, torus.orientation);
    Real direction_squared = dot(ray_direction, ray_direction);
    Real direction_length = real_sqrt(direction_squared);
    Real radius = torus.radius;
    Real shift = -dot(relative, ray_direction) / direction_squared;
    Real parameter_scale = radius / direction_length;
    Vec3 origin = (relative + ray_direction * shift) / radius;
    Vec3 direction = ray_direction / direction_length;
    Real distance_squared = dot(origin, origin);
    Real span_squared = 1.0f - distance_squared;
    if (span_squared < -RealEpsilon * (1.0f + distance_squared))
        return -1.0f;
    Real span = real_sqrt(real_max(0.0f, span_squared));
    Real lower = real_max(-span, (t_min - shift) / parameter_scale);
    Real upper = real_min(span, (t_max - shift) / parameter_scale);
    if (lower > upper)
        return -1.0f;

    Real major = torus.scaled_major_radius;
    Real minor = torus.scaled_minor_radius;
    Real g2 = dot(direction, direction);
    Real g1 = 2.0f * dot(origin, direction);
    Real rho = real_sqrt(origin.x * origin.x + origin.z * origin.z);
    Real vertical = (origin.y - minor) * (origin.y + minor);
    Real radial = (rho - major) * (rho + major) + vertical;
    Real k = 4.0f * major * major;
    Real a4 = g2 * g2;
    Real a3 = 2.0f * g2 * g1;
    Real a2 = g1 * g1 + 2.0f * g2 * radial + k * direction.y * direction.y;
    Real a1 = 2.0f * g1 * radial + 2.0f * k * origin.y * direction.y;
    Real a0 =
        ((rho - major) * (rho - major) + vertical) * ((rho + major) * (rho + major) + vertical);
    QuarticRoots roots;
    int count = solve_quartic(a4, a3, a2, a1, a0, roots);
    for (int i = 0; i < count; ++i) {
        Real root = i == 0 ? roots.r0 : i == 1 ? roots.r1 : i == 2 ? roots.r2 : roots.r3;
        Real value = (((a4 * root + a3) * root + a2) * root + a1) * root + a0;
        while (true) {
            Real slope = ((4.0f * a4 * root + 3.0f * a3) * root + 2.0f * a2) * root + a1;
            if (slope == 0.0f)
                break;
            Real next = root - value / slope;
            if (next == root || next < lower || next > upper || !std::isfinite(float(next)))
                break;
            Real residual = (((a4 * next + a3) * next + a2) * next + a1) * next + a0;
            if (real_abs(residual) >= real_abs(value))
                break;
            root = next;
            value = residual;
        }
        Real t = shift + root * parameter_scale;
        if (t > t_min && t < t_max)
            return t;
    }
    return -1.0f;
}

HD int intersect_tori(const Ray &ray, const Scene &scene, Real t_min, Real &t_max) {
    // each torus inside own cell, first hit is nearest
    Real half_x = 0.5f * TorusColumns * TorusSpacing;
    Real half_z = 0.5f * TorusRows * TorusSpacing;
    Real enter = t_min;
    Real exit = t_max;
    // Restrict the grid walk to heights occupied by tori, especially for shadow rays.
    if (ray.direction.y == 0.0f) {
        if (ray.origin.y < scene.torus_min_y || ray.origin.y > scene.torus_max_y)
            return -1;
    } else {
        Real first = (scene.torus_min_y - ray.origin.y) / ray.direction.y;
        Real last = (scene.torus_max_y - ray.origin.y) / ray.direction.y;
        enter = real_max(enter, real_min(first, last));
        exit = real_min(exit, real_max(first, last));
    }
    if (enter > exit)
        return -1;
    for (int axis = 0; axis < 2; ++axis) {
        Real origin = axis == 0 ? ray.origin.x : ray.origin.z;
        Real direction = axis == 0 ? ray.direction.x : ray.direction.z;
        Real half = axis == 0 ? half_x : half_z;
        if (direction == 0.0f) {
            if (real_abs(origin) > half)
                return -1;
        } else {
            Real first = (-half - origin) / direction;
            Real last = (half - origin) / direction;
            enter = real_max(enter, real_min(first, last));
            exit = real_min(exit, real_max(first, last));
        }
    }
    if (enter > exit)
        return -1;

    Vec3 entry = ray.origin + ray.direction * enter;
    int column =
        (int)real_min(TorusColumns - 1, real_max(0.0f, floorf((entry.x + half_x) / TorusSpacing)));
    int row = (int)real_min(TorusRows - 1, real_max(0.0f, floorf((entry.z + half_z) / TorusSpacing)));
    int step_x = ray.direction.x < 0.0f ? -1 : 1;
    int step_z = ray.direction.z < 0.0f ? -1 : 1;
    Real delta_x = ray.direction.x == 0.0f ? Real(RealMax) : TorusSpacing / real_abs(ray.direction.x);
    Real delta_z = ray.direction.z == 0.0f ? Real(RealMax) : TorusSpacing / real_abs(ray.direction.z);
    Real next_x =
        ray.direction.x == 0.0f
            ? Real(RealMax)
            : ((column + (step_x > 0)) * TorusSpacing - half_x - ray.origin.x) / ray.direction.x;
    Real next_z =
        ray.direction.z == 0.0f
            ? Real(RealMax)
            : ((row + (step_z > 0)) * TorusSpacing - half_z - ray.origin.z) / ray.direction.z;
    while (column >= 0 && column < TorusColumns && row >= 0 && row < TorusRows) {
        int index = row * TorusColumns + column;
        Real t = intersect_torus(ray, scene.tori[index], t_min, t_max);
        if (t >= 0.0f) {
            t_max = t;
            return index;
        }
        Real boundary = real_min(next_x, next_z);
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

HD Real intersect_rectangle(const Ray &ray, const Rectangle &rectangle, Real t_min, Real t_max,
                             Real &u, Real &v) {
    Vec3 normal = rectangle.normal;
    Real denominator = dot(ray.direction, normal);
    if (denominator == 0.0f)
        return -1.0f;
    Real t = dot(rectangle.corner - ray.origin, normal) / denominator;
    if (t <= t_min || t >= t_max)
        return -1.0f;
    Vec3 point = ray.origin + ray.direction * t - rectangle.corner;
    u = dot(point, rectangle.edge_u) * rectangle.inverse_u_squared;
    v = dot(point, rectangle.edge_v) * rectangle.inverse_v_squared;
    return u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f ? t : Real(-1.0f);
}

HD bool occluded(const Ray &ray, const Scene &scene, Real t_min, Real t_max) {
    Real u, v;
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

HD Hit intersect_scene(const Ray &ray, const Scene &scene, Real t_min, Real t_max) {
    Hit hit{};
    hit.t = t_max;
    hit.material_index = -1;
    hit.light_index = -1;
    int torus_index = intersect_tori(ray, scene, t_min, hit.t);
    int rectangle_index = -1;
    if (torus_index >= 0)
        hit.material_index = scene.tori[torus_index].material_index;
    for (int i = 0; i < RectangleCount; ++i) {
        Real u, v;
        Real t = intersect_rectangle(ray, scene.rectangles[i], t_min, hit.t, u, v);
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
        Real u, v;
        Real t = intersect_rectangle(ray, scene.lights[i].surface, t_min, hit.t, u, v);
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
        Real rho = real_sqrt(point.x * point.x + point.z * point.z);
        Real radial = rho - torus.major_radius;
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
