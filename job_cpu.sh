#!/bin/bash -l
#PBS -N raytracer_on_cpu
#PBS -l select=1:ncpus=1
#PBS -l walltime=24:00:00
#PBS -o stats/logs/cpu/
#PBS -e stats/logs/cpu/

set -e -o pipefail
cd "$PBS_O_WORKDIR"
make cpu
./bin/raytracer_cpu | tee "stats/reports/cpu.${PBS_JOBID}.txt"
