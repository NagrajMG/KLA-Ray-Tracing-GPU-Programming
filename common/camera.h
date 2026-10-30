#pragma once

#include "types.h"

Camera make_camera(Vec3 look_from, Vec3 look_at, Vec3 up, float vertical_fov, int width,
                   int height) {
    Camera camera{};
    camera.origin = look_from;
    Vec3 view = look_from - look_at;
    float focus_distance = length(view);
    Vec3 w = view / focus_distance;
    Vec3 u = normalize(cross(up, w));
    Vec3 v = cross(w, u);
    float aspect = (float)width / height;

    float theta = vertical_fov * pi / 180.0f;
    float viewport_height = 2.0f * tanf(theta * 0.5f) * focus_distance;
    Vec3 horizontal = u * (viewport_height * aspect);
    Vec3 downward = v * -viewport_height;

    camera.pixel_du = horizontal / width;
    camera.pixel_dv = downward / height;
    camera.pixel00 = camera.origin - w * focus_distance - (horizontal + downward) * 0.5f +
                     (camera.pixel_du + camera.pixel_dv) * 0.5f;
    return camera;
}

HD Ray ray_for_sample(const Camera &camera, const RenderSettings &settings, int pixel, int sample) {
    // for one pixel, each sample decides its location
    int sample_x = sample % settings.camera_samples_x;
    int sample_y = sample / settings.camera_samples_x;
    // offset from pixel's center
    float offset_x = (sample_x + 0.5f) / settings.camera_samples_x - 0.5f;
    float offset_y = (sample_y + 0.5f) / settings.camera_samples_y - 0.5f;
    // position on the image
    float x = pixel % settings.width + offset_x;
    float y = pixel / settings.width + offset_y;
    Vec3 target = camera.pixel00 + camera.pixel_du * x + camera.pixel_dv * y;
    // final ray connecting the point on the frame and the origin
    return {camera.origin, normalize(target - camera.origin)};
}
