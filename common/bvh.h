#pragma once

#include "types.h"

#include <algorithm>
#include <cfloat>

Bounds torus_bounds(const Torus &torus) {
    float minor = torus.scaled_minor_radius * torus.radius;
    Vec3 extent = torus.orientation == Flat ? Vec3{torus.radius, minor, torus.radius}
                                           : Vec3{torus.radius, torus.radius, minor};
    // Pad bounds for the FP32 intersection solver's rounding near tangencies.
    Vec3 padding{
        32.0f * FLT_EPSILON * fmaxf(1.0f, fabsf(torus.center.x) + torus.radius),
        32.0f * FLT_EPSILON * fmaxf(1.0f, fabsf(torus.center.y) + torus.radius),
        32.0f * FLT_EPSILON * fmaxf(1.0f, fabsf(torus.center.z) + torus.radius)};
    return {torus.center - extent - padding, torus.center + extent + padding};
}

Bounds merge_bounds(Bounds a, Bounds b) {
    return {{fminf(a.minimum.x, b.minimum.x), fminf(a.minimum.y, b.minimum.y),
             fminf(a.minimum.z, b.minimum.z)},
            {fmaxf(a.maximum.x, b.maximum.x), fmaxf(a.maximum.y, b.maximum.y),
             fmaxf(a.maximum.z, b.maximum.z)}};
}

float bvh_component(Vec3 value, int axis) {
    return axis == 0 ? value.x : axis == 1 ? value.y : value.z;
}

// Host-only construction. Median splits keep traversal depth bounded even when
// the tori are moved away from the original regular grid.
int build_torus_bvh_node(Scene &scene, int *indices, int begin, int end, int &next_node) {
    int node_index = next_node++;
    if (end - begin == 1) {
        int torus_index = indices[begin];
        scene.torus_bvh[node_index] = {torus_bounds(scene.tori[torus_index]), torus_index, -1};
        return node_index;
    }

    Vec3 low = scene.tori[indices[begin]].center;
    Vec3 high = low;
    for (int i = begin + 1; i < end; ++i) {
        Vec3 center = scene.tori[indices[i]].center;
        low = {fminf(low.x, center.x), fminf(low.y, center.y), fminf(low.z, center.z)};
        high = {fmaxf(high.x, center.x), fmaxf(high.y, center.y), fmaxf(high.z, center.z)};
    }
    Vec3 span = high - low;
    int axis = span.y > span.x ? 1 : 0;
    if (span.z > bvh_component(span, axis))
        axis = 2;
    int middle = begin + (end - begin) / 2;
    std::nth_element(indices + begin, indices + middle, indices + end, [&](int a, int b) {
        float first = bvh_component(scene.tori[a].center, axis);
        float second = bvh_component(scene.tori[b].center, axis);
        return first == second ? a < b : first < second;
    });
    int left = build_torus_bvh_node(scene, indices, begin, middle, next_node);
    int right = build_torus_bvh_node(scene, indices, middle, end, next_node);
    scene.torus_bvh[node_index] = {
        merge_bounds(scene.torus_bvh[left].bounds, scene.torus_bvh[right].bounds), left, right};
    return node_index;
}

void build_torus_bvh(Scene &scene) {
    int indices[TorusCount];
    for (int i = 0; i < TorusCount; ++i)
        indices[i] = i;
    int next_node = 0;
    build_torus_bvh_node(scene, indices, 0, TorusCount, next_node);
}
