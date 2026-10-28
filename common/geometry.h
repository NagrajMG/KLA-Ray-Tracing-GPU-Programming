#pragma once

#include "types.h"

#include <cfloat>

HD int solve_quadratic(double a, double b, double c, double roots[2]) {
    double discriminant = b * b - 4.0 * a * c;
    double tolerance = DBL_EPSILON * (b * b + fabs(4.0 * a * c));
    if (discriminant < -tolerance)
        return 0;
    if (fabs(discriminant) <= tolerance) {
        roots[0] = -b / (2.0 * a);
        return 1;
    }
    double q = -0.5 * (b + copysign(sqrt(discriminant), b));
    roots[0] = q / a;
    roots[1] = c / q;
    return 2;
}

HD double largest_cubic_root(double a, double b, double c) {
    double p = b - a * a / 3.0;
    double q = 2.0 * a * a * a / 27.0 - a * b / 3.0 + c;
    double third_p = p / 3.0;
    double half_q = q / 2.0;
    double discriminant = half_q * half_q + third_p * third_p * third_p;
    if (discriminant >= 0.0) {
        double u = cbrt(-half_q - copysign(sqrt(discriminant), half_q));
        double v = u == 0.0 ? 0.0 : -third_p / u;
        double root = u + v;
        if (discriminant == 0.0)
            root = fmax(root, -u);
        return root - a / 3.0;
    }
    double radius = sqrt(-third_p);
    double cosine = -half_q / (radius * radius * radius);
    double angle = acos(fmax(-1.0, fmin(1.0, cosine))) / 3.0;
    return 2.0 * radius * cos(angle) - a / 3.0;
}

HD int solve_quartic(double a4, double a3, double a2, double a1, double a0, double roots[4]) {
    double a = a3 / a4;
    double b = a2 / a4;
    double c = a1 / a4;
    double d = a0 / a4;
    double p = b - 3.0 * a * a / 8.0;
    double q = c - a * b / 2.0 + a * a * a / 8.0;
    double r = d - a * c / 4.0 + a * a * b / 16.0 - 3.0 * a * a * a * a / 256.0;
    int count = 0;
    double pair[2];
    if (q == 0.0) {
        int squares = solve_quadratic(1.0, p, r, pair);
        for (int i = 0; i < squares; ++i) {
            if (pair[i] < 0.0)
                continue;
            double root = sqrt(pair[i]);
            roots[count++] = root - a / 4.0;
            if (root != 0.0)
                roots[count++] = -root - a / 4.0;
        }
    } else {
        double cubic_a = 2.0 * p;
        double cubic_b = p * p - 4.0 * r;
        double cubic_c = -q * q;
        double z = fmax(0.0, largest_cubic_root(cubic_a, cubic_b, cubic_c));
        double bound = 1.0 + fmax(fabs(cubic_a), fmax(fabs(cubic_b), fabs(cubic_c)));
        while (true) {
            double value = ((z + cubic_a) * z + cubic_b) * z + cubic_c;
            double slope = (3.0 * z + 2.0 * cubic_a) * z + cubic_b;
            if (slope == 0.0)
                break;
            double next = z - value / slope;
            if (next == z || next < 0.0 || next > bound || !std::isfinite(next))
                break;
            double residual = ((next + cubic_a) * next + cubic_b) * next + cubic_c;
            if (fabs(residual) >= fabs(value))
                break;
            z = next;
        }
        if (z == 0.0)
            return 0;
        double s = sqrt(z);
        int first = solve_quadratic(1.0, s, (p + z - q / s) / 2.0, pair);
        for (int i = 0; i < first; ++i)
            roots[count++] = pair[i] - a / 4.0;
        int second = solve_quadratic(1.0, -s, (p + z + q / s) / 2.0, pair);
        for (int i = 0; i < second; ++i)
            roots[count++] = pair[i] - a / 4.0;
    }
    for (int i = 1; i < count; ++i) {
        double root = roots[i];
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

HD double intersect_torus(const Ray &ray, const Torus &torus, double t_min, double t_max) {
    Vec3 relative = torus_local(ray.origin - torus.center, torus.orientation);
    Vec3 ray_direction = torus_local(ray.direction, torus.orientation);
    double direction_squared = dot(ray_direction, ray_direction);
    double direction_length = sqrt(direction_squared);
    double radius = torus.major_radius + torus.minor_radius;
    double shift = -dot(relative, ray_direction) / direction_squared;
    double parameter_scale = radius / direction_length;
    Vec3 origin = (relative + ray_direction * shift) / radius;
    Vec3 direction = ray_direction / direction_length;
    double distance_squared = dot(origin, origin);
    double span_squared = 1.0 - distance_squared;
    if (span_squared < -DBL_EPSILON * (1.0 + distance_squared))
        return -1.0;
    double span = sqrt(fmax(0.0, span_squared));
    double lower = fmax(-span, (t_min - shift) / parameter_scale);
    double upper = fmin(span, (t_max - shift) / parameter_scale);
    if (lower > upper)
        return -1.0;

    double major = torus.major_radius / radius;
    double minor = torus.minor_radius / radius;
    double g2 = dot(direction, direction);
    double g1 = 2.0 * dot(origin, direction);
    double rho = sqrt(origin.x * origin.x + origin.z * origin.z);
    double vertical = (origin.y - minor) * (origin.y + minor);
    double radial = (rho - major) * (rho + major) + vertical;
    double k = 4.0 * major * major;
    double a4 = g2 * g2;
    double a3 = 2.0 * g2 * g1;
    double a2 = g1 * g1 + 2.0 * g2 * radial + k * direction.y * direction.y;
    double a1 = 2.0 * g1 * radial + 2.0 * k * origin.y * direction.y;
    double a0 =
        ((rho - major) * (rho - major) + vertical) * ((rho + major) * (rho + major) + vertical);
    double roots[4];
    int count = solve_quartic(a4, a3, a2, a1, a0, roots);
    for (int i = 0; i < count; ++i) {
        double root = roots[i];
        while (true) {
            double value = (((a4 * root + a3) * root + a2) * root + a1) * root + a0;
            double slope = ((4.0 * a4 * root + 3.0 * a3) * root + 2.0 * a2) * root + a1;
            if (slope == 0.0)
                break;
            double next = root - value / slope;
            if (next == root || next < lower || next > upper || !std::isfinite(next))
                break;
            double residual = (((a4 * next + a3) * next + a2) * next + a1) * next + a0;
            if (fabs(residual) >= fabs(value))
                break;
            root = next;
        }
        double t = shift + root * parameter_scale;
        if (t > t_min && t < t_max)
            return t;
    }
    return -1.0;
}

HD double intersect_rectangle(const Ray &ray, const Rectangle &rectangle, double t_min,
                              double t_max, double &u, double &v) {
    Vec3 normal = cross(rectangle.edge_u, rectangle.edge_v);
    double denominator = dot(ray.direction, normal);
    if (denominator == 0.0)
        return -1.0;
    double t = dot(rectangle.corner - ray.origin, normal) / denominator;
    if (t <= t_min || t >= t_max)
        return -1.0;
    Vec3 point = ray.origin + ray.direction * t - rectangle.corner;
    u = dot(point, rectangle.edge_u) / dot(rectangle.edge_u, rectangle.edge_u);
    v = dot(point, rectangle.edge_v) / dot(rectangle.edge_v, rectangle.edge_v);
    return u >= 0.0 && u <= 1.0 && v >= 0.0 && v <= 1.0 ? t : -1.0;
}

HD Hit intersect_scene(const Ray &ray, const Scene &scene, double t_min, double t_max) {
    Hit hit{};
    hit.t = t_max;
    hit.material_index = -1;
    hit.light_index = -1;
    int torus_index = -1;
    int rectangle_index = -1;
    for (int i = 0; i < TorusCount; ++i) {
        double t = intersect_torus(ray, scene.tori[i], t_min, hit.t);
        if (t < 0.0)
            continue;
        hit.t = t;
        hit.material_index = scene.tori[i].material_index;
        torus_index = i;
    }
    for (int i = 0; i < RectangleCount; ++i) {
        double u, v;
        double t = intersect_rectangle(ray, scene.rectangles[i], t_min, hit.t, u, v);
        if (t < 0.0)
            continue;
        hit.t = t;
        hit.u = u;
        hit.v = v;
        hit.material_index = scene.rectangles[i].material_index;
        rectangle_index = i;
        torus_index = -1;
    }
    for (int i = 0; i < LightCount; ++i) {
        double u, v;
        double t = intersect_rectangle(ray, scene.lights[i].surface, t_min, hit.t, u, v);
        if (t < 0.0)
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
        double rho = sqrt(point.x * point.x + point.z * point.z);
        double radial = rho - torus.major_radius;
        Vec3 normal = normalize(Vec3{point.x * radial / rho, point.y, point.z * radial / rho});
        hit.normal = torus_world(normal, torus.orientation);
    } else {
        const Rectangle &rectangle = scene.rectangles[rectangle_index];
        hit.normal = normalize(cross(rectangle.edge_u, rectangle.edge_v));
    }
    if (dot(ray.direction, hit.normal) > 0.0) {
        hit.normal = hit.normal * -1.0;
    }
    return hit;
}
