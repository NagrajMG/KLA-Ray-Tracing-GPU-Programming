#!/bin/bash -l
#PBS -N raytracer_on_gpu
#PBS -l select=1:ncpus=1:ngpus=1
#PBS -l walltime=01:00:00
#PBS -q gpuq
#PBS -o stats/logs/gpu/
#PBS -e stats/logs/gpu/

set -e -o pipefail
cd "$PBS_O_WORKDIR"
module load cuda11.4
make gpu
./bin/raytracer_gpu | tee "stats/reports/gpu.${PBS_JOBID}.txt"
