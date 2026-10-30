# Tori Ray-Tracer

Serial C++ and CUDA renderers for a scene of 64 tori.

![GPU render](images/gpu/1280_720.png)

Build with `make cpu` or `make gpu`.

On Aqua, submit `qsub job_cpu.sh` or `qsub job_gpu.sh` from the repository root.

## Camera

Camera, pixel steps and view basis.

![Camera and viewport](assets/viewport.png)

![Pixel centres and steps](assets/pixel-steps.png)

![Camera basis](assets/basis.png)

## Rendering flow

```mermaid
flowchart TD
    A["Create scene, textures and camera"] --> B["Choose a pixel"]
    B --> C["Generate 16 camera rays"]
    C --> D["Trace rays: intersections, lighting, shadows and reflections"]
    D --> E["Average sample colours"]
    E --> F["Store linear RGB"]
    F --> G["After all pixels, convert and write image"]
```
