.PHONY: all cpu cuda clean

all: cpu cuda

cpu: src/raytracer_cpu.cpp
	c++ -O3 -std=c++17 src/raytracer_cpu.cpp -o raytracer_cpu

cuda: src/raytracer_cuda.cu
	nvcc -O3 -std=c++17 src/raytracer_cuda.cu -o raytracer_cuda

clean:
	rm -f raytracer_cpu raytracer_cuda
