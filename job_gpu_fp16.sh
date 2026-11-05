#!/bin/bash -l
#PBS -N raytracer_fp16
#PBS -l select=1:ncpus=1:ngpus=1
#PBS -l walltime=01:00:00
#PBS -q gpuq
#PBS -S /bin/bash
#PBS -j oe

# Log before module setup or compilation. Avoid nounset while running the site's
# module initialization, which may legitimately inspect unset environment vars.
set -Ee -o pipefail
trap 'status=$?; printf "ERROR: exit %s at line %s: %s\n" "$status" "$LINENO" "$BASH_COMMAND" >&2; exit "$status"' ERR
trap 'status=$?; printf "Job finished: %s; exit status: %s\n" "$(date -Is)" "$status"' EXIT

printf 'Job started: %s; host: %s; job ID: %s\n' "$(date -Is)" "$(hostname)" "${PBS_JOBID:-manual}"
if [[ -z "${PBS_O_WORKDIR:-}" ]]; then
    echo "ERROR: PBS_O_WORKDIR is unset. Submit with qsub from the repository root." >&2
    exit 1
fi
cd "$PBS_O_WORKDIR"

# V100 = sm_70. Override with qsub -v GPU_ARCH=sm_86 for an RTX 3050.
gpu_arch="${GPU_ARCH:-sm_70}"
job_id="${PBS_JOBID:-manual.$(date +%Y%m%d_%H%M%S).$$}"
run_dir="$PWD/stats/reports/fp16.${job_id}"
image_dir="$PWD/images/gpu/fp16.${job_id}"
mkdir -p "$run_dir" "$image_dir"
echo "Job log: $run_dir/job.log"
exec > >(tee -a "$run_dir/job.log") 2>&1
printf 'Working directory: %s\nGPU architecture: %s\n' "$PWD" "$gpu_arch"
if [[ ! -f Makefile || ! -f gpu/main.cu || ! -f compare_precision.py ]]; then
    echo "ERROR: Submit from the repository root, with all FP16 files copied to AQUA." >&2
    exit 1
fi

echo "Loading cuda11.4..."
module load cuda11.4
set -u
echo "CUDA module loaded. Collecting device/compiler information..."
nvidia-smi > "$run_dir/device.txt"
nvcc --version > "$run_dir/compiler.txt"
# Source metadata is useful but must not stop a copied working tree from running.
if git rev-parse HEAD > "$run_dir/commit.txt" 2> "$run_dir/git-errors.txt"; then
    git diff > "$run_dir/source.diff" 2>> "$run_dir/git-errors.txt" || true
else
    echo "Git metadata unavailable; continuing with the files in $PWD."
fi

# Force rebuilding: make does not track changes to GPU_ARCH between machines.
echo "Building the three precision variants..."
make -B GPU_ARCH="$gpu_arch" gpu gpu-fp16-color gpu-fp16-geometry 2>&1 | tee "$run_dir/build.txt"

echo "Rendering FP32 reference..."
./bin/raytracer_gpu "$image_dir/fp32.ppm" "$image_dir/fp32.linear.f32" \
    2>&1 | tee "$run_dir/fp32.timing.txt"
echo "Rendering FP16 colours..."
./bin/raytracer_gpu_fp16_color "$image_dir/fp16_color.ppm" "$image_dir/fp16_color.linear.f16" \
    2>&1 | tee "$run_dir/fp16_color.timing.txt"
echo "Rendering experimental FP16 geometry..."
./bin/raytracer_gpu_fp16_geometry "$image_dir/fp16_geometry.ppm" "$image_dir/fp16_geometry.linear.f16" \
    2>&1 | tee "$run_dir/fp16_geometry.timing.txt"

# Uses only Python's standard library; comparison can also run off-cluster.
# Some AQUA environments provide no Python until a separate module is loaded.
python_cmd=""
for python_candidate in "${PYTHON:-python3}" python; do
    if command -v "$python_candidate" > /dev/null 2>&1 && \
        "$python_candidate" -c 'import sys, struct; sys.exit(0 if sys.version_info >= (3, 6) and struct.calcsize("e") == 2 else 1)' > /dev/null 2>&1; then
        python_cmd="$python_candidate"
        break
    fi
done
if [[ -n "$python_cmd" ]]; then
    "$python_cmd" compare_precision.py "$image_dir/fp32.ppm" "$image_dir/fp16_color.ppm" \
        --reference-linear "$image_dir/fp32.linear.f32" \
        --candidate-linear "$image_dir/fp16_color.linear.f16" \
        2>&1 | tee "$run_dir/fp16_color.comparison.txt"
    "$python_cmd" compare_precision.py "$image_dir/fp32.ppm" "$image_dir/fp16_geometry.ppm" \
        --reference-linear "$image_dir/fp32.linear.f32" \
        --candidate-linear "$image_dir/fp16_geometry.linear.f16" \
        2>&1 | tee "$run_dir/fp16_geometry.comparison.txt"
else
    printf '%s\n' \
        'SKIPPED: image comparisons require Python 3.6 or newer.' \
        'All three renders completed; saved images and timings remain available.' \
        'Run compare_precision.py on these saved files on a machine with Python.' \
        | tee "$run_dir/fp16_color.comparison.txt" "$run_dir/fp16_geometry.comparison.txt"
fi

echo "Images: $image_dir"
echo "Reports: $run_dir"
