#include "../common/geometry.h"
#include "../common/scene.h"

#include <cassert>
#include <iostream>
#include <random>

int brute_force(const Ray &ray, const Scene &scene, float t_min, float &t_max) {
    int nearest = -1;
    for (int i = 0; i < TorusCount; ++i) {
        float t = intersect_torus(ray, scene.tori[i], t_min, t_max);
        if (t >= 0.0f) {
            nearest = i;
            t_max = t;
        }
    }
    return nearest;
}

void check_ray(const Ray &ray, const Scene &scene, float t_min, float t_max) {
    float expected_t = t_max;
    int expected = brute_force(ray, scene, t_min, expected_t);
    float actual_t = t_max;
    int actual = intersect_tori(ray, scene, t_min, actual_t);
    if ((expected < 0) != (actual < 0) || fabsf(expected_t - actual_t) > 1e-4f) {
        std::cerr << "BVH mismatch: expected " << expected << " at " << expected_t
                  << ", got " << actual << " at " << actual_t << '\n';
        std::abort();
    }
    assert(occluded_tori(ray, scene, t_min, t_max) == (expected >= 0));
}

void check_tree(const Scene &scene, int node_index, int depth, bool *leaves, bool *nodes) {
    assert(node_index >= 0 && node_index < TorusBvhNodeCount);
    assert(!nodes[node_index]);
    nodes[node_index] = true;
    assert(depth <= TorusBvhDepth);
    const BvhNode &node = scene.torus_bvh[node_index];
    if (node.second < 0) {
        assert(node.first >= 0 && node.first < TorusCount && !leaves[node.first]);
        leaves[node.first] = true;
        Bounds expected = torus_bounds(scene.tori[node.first]);
        assert(node.bounds.minimum.x == expected.minimum.x);
        assert(node.bounds.minimum.y == expected.minimum.y);
        assert(node.bounds.minimum.z == expected.minimum.z);
        assert(node.bounds.maximum.x == expected.maximum.x);
        assert(node.bounds.maximum.y == expected.maximum.y);
        assert(node.bounds.maximum.z == expected.maximum.z);
        return;
    }
    for (int child : {node.first, node.second}) {
        assert(child >= 0 && child < TorusBvhNodeCount);
        const Bounds &bounds = scene.torus_bvh[child].bounds;
        assert(node.bounds.minimum.x <= bounds.minimum.x && node.bounds.maximum.x >= bounds.maximum.x);
        assert(node.bounds.minimum.y <= bounds.minimum.y && node.bounds.maximum.y >= bounds.maximum.y);
        assert(node.bounds.minimum.z <= bounds.minimum.z && node.bounds.maximum.z >= bounds.maximum.z);
        check_tree(scene, child, depth + 1, leaves, nodes);
    }
}

void check_scene(const Scene &scene) {
    bool leaves[TorusCount]{};
    bool nodes[TorusBvhNodeCount]{};
    check_tree(scene, 0, 0, leaves, nodes);
    for (bool visited : leaves)
        assert(visited);
    for (bool visited : nodes)
        assert(visited);

    std::mt19937 rng(6023);
    std::uniform_real_distribution<float> position(-30.0f, 30.0f);
    std::uniform_real_distribution<float> direction(-1.0f, 1.0f);
    std::uniform_real_distribution<float> limit(0.01f, 70.0f);
    for (int i = 0; i < 30000; ++i) {
        Ray ray{{position(rng), position(rng), position(rng)},
                normalize(Vec3{direction(rng), direction(rng), direction(rng)})};
        // Exercise axis-parallel rays, signed zero and finite shadow intervals.
        if (i % 5 == 0) ray.direction.x = 0.0f;
        if (i % 7 == 0) ray.direction.y = -0.0f;
        check_ray(ray, scene, 1e-5f, i % 2 == 0 ? FLT_MAX : limit(rng));
    }
    for (const Torus &torus : scene.tori) {
        float minor = torus.scaled_minor_radius * torus.radius;
        for (float offset : {-torus.radius, -minor, 0.0f, minor, torus.radius}) {
            // Include rays starting inside bounds and near tangent surfaces.
            for (float epsilon : {-1e-5f, 0.0f, 1e-5f}) {
                check_ray({torus.center + Vec3{offset + epsilon, 0.0f, 8.0f}, {0, 0, -1}}, scene, 1e-5f, 20.0f);
                check_ray({torus.center + Vec3{0.0f, offset + epsilon, 0.0f}, {1, 0, 0}}, scene, 1e-5f, 20.0f);
                check_ray({torus.center + Vec3{8.0f, offset + epsilon, 0.0f}, {-1, 0, 0}}, scene, 1e-5f, 20.0f);
            }
        }
    }
}

void check_bounds() {
    Bounds bounds{{-1, -1, -1}, {1, 1, 1}};
    float entry;
    assert(intersect_bounds({{-2, 0, 0}, {1, 0, 0}}, {1, 0, 0}, bounds, 0, 10, entry));
    assert(entry == 1.0f);
    assert(!intersect_bounds({{-2, 2, 0}, {1, -0.0f, 0}}, {1, 0, 0}, bounds, 0, 10, entry));
    assert(!intersect_bounds({{-2, 0, 0}, {1, 0, 0}}, {1, 0, 0}, bounds, 0, 0.5f, entry));
    float tiny = FLT_MIN / 8.0f;
    assert(intersect_bounds({{-1, 0, 0}, {tiny, 0, 0}}, {INFINITY, 0, 0}, bounds, 0, 10, entry));
    assert(entry == 0.0f);
}

int main() {
    check_bounds();
    std::vector<float> textures;
    Scene scene = make_scene(textures);
    check_scene(scene);

    // BVH correctness must not rely on the old one-torus-per-grid-cell layout.
    for (int i = 0; i < TorusCount; ++i) {
        scene.tori[i].center.x += 0.4f * (i % 5);
        scene.tori[i].center.y += 0.6f * (i % 7);
        scene.tori[i].center.z -= 0.3f * (i % 3);
    }
    build_torus_bvh(scene);
    check_scene(scene);

    // Coincident centroids must also produce a balanced tree and fit the stack.
    for (Torus &torus : scene.tori)
        torus.center = {0, 2, 0};
    build_torus_bvh(scene);
    check_scene(scene);
    std::cout << "BVH nearest-hit/any-hit queries match brute force; bounds and tree checks passed\n";
}
