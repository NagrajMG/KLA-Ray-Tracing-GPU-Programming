HEADERS := $(wildcard common/*.h)

.PHONY: all cpu gpu clean format

all: cpu gpu

cpu: bin/raytracer_cpu

gpu: bin/raytracer_gpu

bin/raytracer_cpu: cpu/main.cpp $(HEADERS)
	mkdir -p bin
	c++ -O3 -std=c++17 cpu/main.cpp -o $@

bin/raytracer_gpu: gpu/main.cu $(HEADERS)
	mkdir -p bin
	nvcc -O3 -std=c++17 -lineinfo -arch=sm_86 gpu/main.cu -o $@

format:
	clang-format -i cpu/main.cpp gpu/main.cu $(HEADERS)

clean:
	rm -f bin/raytracer_cpu bin/raytracer_gpu
