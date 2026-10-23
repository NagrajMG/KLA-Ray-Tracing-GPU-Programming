#!/bin/bash -l
#PBS -N raytracer_blocks
#PBS -l select=1:ncpus=1:ngpus=1
#PBS -l walltime=00:10:00
#PBS -q gpuq
#PBS -o raytracer_blocks.log
#PBS -e raytracer_blocks.err

set -eo pipefail

cd "$PBS_O_WORKDIR"
module load cuda11.4

# Keep each submission's executable, measurements, and images together.
results_dir="$PBS_O_WORKDIR/block_results/$PBS_JOBID"
mkdir -p "$results_dir"
nvcc --version > "$results_dir/cuda_version.txt"
nvidia-smi > "$results_dir/gpu_info.txt"
cat "$results_dir/gpu_info.txt"

# Block dimensions are runtime arguments, so all configurations use one binary.
# ptxas reports registers, stack frames, and spill loads/stores at compile time.
nvcc -O3 -std=c++17 -arch=sm_70 -Xptxas=-v src/raytracer_cuda.cu \
    -o "$results_dir/raytracer_cuda" 2>&1 | tee "$results_dir/compiler.log"

printf 'block_x,block_y,threads,repeat,kernel_ms,image_matches_baseline\n' > "$results_dir/timings.csv"
blocks=("16 16" "8 8" "16 8" "32 4" "32 8" "32 16")

# Five independent batches per shape; each executable run performs two warmups
# and reports the average of 20 timed renders. Rotate order between repeats.
for repeat in {1..5}; do
    for offset in "${!blocks[@]}"; do
        index=$(( (offset + repeat - 1) % ${#blocks[@]} ))
        read -r bx by <<< "${blocks[index]}"
        run_dir="$results_dir/${bx}x${by}/repeat_$repeat"
        mkdir -p "$run_dir"
        (
            cd "$run_dir"
            "$results_dir/raytracer_cuda" "$bx" "$by" > run.log
        )
        ms=$(awk '/^GPU kernel time:/ {print $4}' "$run_dir/run.log")
        if [[ ! "$ms" =~ ^[0-9]+([.][0-9]+)?$ ]]; then
            echo "Missing kernel timing: $run_dir/run.log" >&2
            exit 1
        fi
        matches=no
        if cmp -s "$results_dir/16x16/repeat_1/gpu.ppm" "$run_dir/gpu.ppm"; then
            matches=yes
        fi
        printf '%s,%s,%s,%s,%s,%s\n' "$bx" "$by" "$((bx * by))" "$repeat" "$ms" "$matches" \
            | tee -a "$results_dir/timings.csv"
    done
done

# Summarize the five batch averages; speedup uses the median 16x16 time.
awk -F, '
    NR == 1 { next }
    {
        key = $1 "x" $2
        if (!(key in count)) order[++shapes] = key
        times[key, ++count[key]] = $5
        if ($6 != "yes") mismatches[key]++
    }
    END {
        for (s = 1; s <= shapes; s++) {
            key = order[s]
            for (i = 1; i <= count[key]; i++) {
                for (j = i + 1; j <= count[key]; j++) {
                    if (times[key, i] > times[key, j]) {
                        tmp = times[key, i]
                        times[key, i] = times[key, j]
                        times[key, j] = tmp
                    }
                }
            }
            median[key] = times[key, 3]
        }
        print "block,median_ms,min_ms,max_ms,speedup_vs_16x16,image_mismatches"
        for (s = 1; s <= shapes; s++) {
            key = order[s]
            speedup = median[key] > 0 ? sprintf("%.3f", median["16x16"] / median[key]) : "NA"
            printf "%s,%.3f,%.3f,%.3f,%s,%d\n", key, median[key], times[key, 1], times[key, 5], speedup, mismatches[key]
        }
    }
' "$results_dir/timings.csv" | tee "$results_dir/summary.csv"

echo "Results saved in $results_dir"
