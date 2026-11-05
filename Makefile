HEADERS := $(wildcard common/*.h)
NVCC ?= nvcc
GPU_ARCH ?= sm_86

.PHONY: all cpu gpu test clean format

all: cpu gpu

cpu: bin/raytracer_cpu

gpu: bin/raytracer_gpu

bin/raytracer_cpu: cpu/main.cpp $(HEADERS)
	mkdir -p bin
	c++ -O3 -std=c++17 cpu/main.cpp -o $@

bin/raytracer_gpu: gpu/main.cu $(HEADERS)
	mkdir -p bin
	$(NVCC) -O3 -std=c++17 -lineinfo -arch=$(GPU_ARCH) gpu/main.cu -o $@

test: bin/test_bvh
	./bin/test_bvh

bin/test_bvh: tests/bvh.cpp $(HEADERS)
	mkdir -p bin
	c++ -O3 -std=c++17 tests/bvh.cpp -o $@

format:
	clang-format -i cpu/main.cpp gpu/main.cu $(HEADERS)

clean:
	rm -f bin/raytracer_cpu bin/raytracer_gpu bin/test_bvh
