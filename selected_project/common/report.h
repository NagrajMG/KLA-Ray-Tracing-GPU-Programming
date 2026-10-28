#pragma once

#include "types.h"

#include <iomanip>
#include <iostream>

void print_report_header(const char *backend, const RenderSettings &settings, int warmup_runs,
                         const char *timing_scope, const char *image_path) {
    std::cout << "Ray tracer benchmark\n"
              << "Backend                 : " << backend << '\n'
              << "Tori                    : " << TorusCount << '\n'
              << "Room surfaces           : " << RectangleCount << '\n'
              << "Area lights             : " << LightCount << '\n'
              << "Resolution              : " << settings.width << " x " << settings.height << '\n'
              << "Camera samples          : "
              << settings.camera_samples_x * settings.camera_samples_y << " ("
              << settings.camera_samples_x << " x " << settings.camera_samples_y << ")\n"
              << "Samples per light       : " << settings.light_samples_x * settings.light_samples_y
              << " (" << settings.light_samples_x << " x " << settings.light_samples_y << ")\n"
              << "Reflection depth        : " << settings.reflection_depth << '\n'
              << "Geometry precision      : double\n"
              << "Colour precision        : float\n"
              << "Ray epsilon             : " << settings.ray_epsilon << '\n'
              << "Timed runs              : " << settings.runs << '\n'
              << "Warm-up runs            : " << warmup_runs << '\n'
              << "Timing scope            : " << timing_scope << '\n'
              << "Image                   : " << image_path << '\n'
              << std::flush;
}

void print_render_time(double average_ms) {
    std::cout << std::fixed << std::setprecision(3) << "Average render time (ms): " << average_ms
              << '\n';
}
