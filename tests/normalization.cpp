#include "../common/scene.h"

#include <cassert>
#include <iostream>

void check_unit(Vec3 normal) {
    float x = float(normal.x);
    float y = float(normal.y);
    float z = float(normal.z);
    assert(std::isfinite(x) && std::isfinite(y) && std::isfinite(z));
    assert(fabsf(x * x + y * y + z * z - 1.0f) < 0.003f);
}

int main() {
    std::vector<ColorScalar> textures;
    Scene scene = make_scene(textures);
    for (const Rectangle &rectangle : scene.rectangles)
        check_unit(normalize(rectangle.normal));

    // A side-wall mirror must reverse X while preserving Y and Z.
    Vec3 direction = normalize(Vec3{0.5f, -0.2f, -0.84f});
    Vec3 reflected = reflect(direction, normalize(scene.rectangles[2].normal));
    check_unit(reflected);
    assert(fabsf(float(reflected.x) + float(direction.x)) < 0.003f);
    assert(fabsf(float(reflected.y) - float(direction.y)) < 0.003f);
    assert(fabsf(float(reflected.z) - float(direction.z)) < 0.003f);

#ifdef RT_FP16_GEOMETRY
    check_unit(normalize(Vec3{65504.0f, 65504.0f, 65504.0f}));
    check_unit(normalize(Vec3{5.960464477539063e-8f, 0.0f, 0.0f}));
    Vec3 zero = normalize(Vec3{0.0f, 0.0f, 0.0f});
    assert(float(zero.x) == 0.0f && float(zero.y) == 0.0f && float(zero.z) == 0.0f);
#endif

    std::cout << "Room normals and mirror reflection checks passed\n";
}
