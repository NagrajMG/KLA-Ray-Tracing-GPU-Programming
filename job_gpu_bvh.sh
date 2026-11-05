#!/bin/bash -l
#PBS -N raytracer_bvh
#PBS -l select=1:ncpus=1:ngpus=1
#PBS -l walltime=01:00:00
#PBS -q gpuq
#PBS -S /bin/bash
#PBS -j oe

# Set up logging before loading modules; some module functions read unset vars.
set -Ee -o pipefail
trap 'status=$?; printf "ERROR: exit %s at line %s: %s\n" "$status" "$LINENO" "$BASH_COMMAND" >&2; exit "$status"' ERR
trap 'status=$?; printf "Job finished: %s; exit status: %s\n" "$(date -Is)" "$status"' EXIT
printf 'Job started: %s; host: %s; job ID: %s\n' "$(date -Is)" "$(hostname)" "${PBS_JOBID:-manual}"
if [[ -z "${PBS_O_WORKDIR:-}" ]]; then
    echo "ERROR: Submit with qsub from the repository root." >&2
    exit 1
fi
cd "$PBS_O_WORKDIR"

gpu_arch="${GPU_ARCH:-sm_70}"
job_id="${PBS_JOBID:-manual.$(date +%Y%m%d_%H%M%S).$$}"
run_dir="$PWD/stats/reports/bvh.${job_id}"
image_dir="$PWD/images/gpu/bvh.${job_id}"
mkdir -p "$run_dir" "$image_dir"
echo "Job log: $run_dir/job.log"
exec > >(tee -a "$run_dir/job.log") 2>&1
printf 'Working directory: %s\nGPU architecture: %s\n' "$PWD" "$gpu_arch"
if [[ ! -f Makefile || ! -f common/bvh.h || ! -f gpu/main.cu ]]; then
    echo "ERROR: Copy the BVH sources and submit from the repository root." >&2
    exit 1
fi

echo "Loading cuda11.4..."
module load cuda11.4
set -u
nvidia-smi > "$run_dir/device.txt"
nvcc --version > "$run_dir/compiler.txt"
if git rev-parse HEAD > "$run_dir/commit.txt" 2> "$run_dir/git-errors.txt"; then
    git diff > "$run_dir/source.diff" 2>> "$run_dir/git-errors.txt" || true
else
    echo "Git metadata unavailable; continuing with the copied sources."
fi

# Force rebuilding when moving between V100 (sm_70) and RTX 3050 (sm_86).
echo "Building BVH renderer..."
make -B GPU_ARCH="$gpu_arch" gpu 2>&1 | tee "$run_dir/build.txt"
echo "Rendering with BVH..."
./bin/raytracer_gpu "$image_dir/render.ppm" 2>&1 | tee "$run_dir/bvh.timing.txt"
echo "Image: $image_dir/render.ppm"
echo "Reports: $run_dir"
