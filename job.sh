#!/bin/bash -l
#PBS -N kla_raytracer
#PBS -l select=1:ncpus=1:ngpus=1
#PBS -l walltime=00:10:00
#PBS -q gpuq
#PBS -o raytracer.log
#PBS -e raytracer.err

set -e

cd "$PBS_O_WORKDIR"
module load cuda11.4
nvcc -O3 -std=c++14 src/raytracer_cuda.cu -o raytracer_cuda
./raytracer_cuda
