HEADERS := $(wildcard common/*.h)
NVCC ?= nvcc
GPU_ARCH ?= sm_86
GPU_FLAGS := -O3 -std=c++17 -lineinfo -arch=$(GPU_ARCH)

.PHONY: all cpu gpu gpu-fp16-color gpu-fp16-geometry clean format

all: cpu gpu

cpu: bin/raytracer_cpu

gpu: bin/raytracer_gpu

gpu-fp16-color: bin/raytracer_gpu_fp16_color

gpu-fp16-geometry: bin/raytracer_gpu_fp16_geometry

bin/raytracer_cpu: cpu/main.cpp $(HEADERS)
	mkdir -p bin
	c++ -O3 -std=c++17 cpu/main.cpp -o $@

bin/raytracer_gpu: gpu/main.cu $(HEADERS)
	mkdir -p bin
	$(NVCC) $(GPU_FLAGS) gpu/main.cu -o $@

bin/raytracer_gpu_fp16_color: gpu/main.cu $(HEADERS)
	mkdir -p bin
	$(NVCC) $(GPU_FLAGS) -DRT_FP16_COLORS gpu/main.cu -o $@

bin/raytracer_gpu_fp16_geometry: gpu/main.cu $(HEADERS)
	mkdir -p bin
	$(NVCC) $(GPU_FLAGS) -DRT_FP16_COLORS -DRT_FP16_GEOMETRY gpu/main.cu -o $@

format:
	clang-format -i cpu/main.cpp gpu/main.cu $(HEADERS)

clean:
	rm -f bin/raytracer_cpu bin/raytracer_gpu bin/raytracer_gpu_fp16_color bin/raytracer_gpu_fp16_geometry
