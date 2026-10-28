#pragma once

#include "types.h"

Camera make_camera(Vec3 look_from, Vec3 look_at, Vec3 up, double vertical_fov, int width,
                   int height) {
    Camera camera{};
    camera.origin = look_from;
    Vec3 view = look_from - look_at;
    double focus_distance = length(view);
    Vec3 w = view / focus_distance;
    Vec3 u = normalize(cross(up, w));
    Vec3 v = cross(w, u);
    double aspect = (double)width / height;

    double theta = vertical_fov * pi / 180.0;
    double viewport_height = 2.0 * tan(theta * 0.5) * focus_distance;
    Vec3 horizontal = u * (viewport_height * aspect);
    Vec3 downward = v * -viewport_height;

    camera.pixel_du = horizontal / width;
    camera.pixel_dv = downward / height;
    camera.pixel00 = camera.origin - w * focus_distance - (horizontal + downward) * 0.5 +
                     (camera.pixel_du + camera.pixel_dv) * 0.5;
    return camera;
}

HD Ray ray_for_sample(const Camera &camera, const RenderSettings &settings, int pixel, int sample) {
    int sample_x = sample % settings.camera_samples_x;
    int sample_y = sample / settings.camera_samples_x;
    double offset_x = (sample_x + 0.5) / settings.camera_samples_x - 0.5;
    double offset_y = (sample_y + 0.5) / settings.camera_samples_y - 0.5;
    double x = pixel % settings.width + offset_x;
    double y = pixel / settings.width + offset_y;
    Vec3 target = camera.pixel00 + camera.pixel_du * x + camera.pixel_dv * y;
    return {camera.origin, normalize(target - camera.origin)};
}
