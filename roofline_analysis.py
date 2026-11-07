import argparse
import csv
from pathlib import Path
import re
from statistics import median

import matplotlib.pyplot as plt
import numpy as np


ROOT = Path(__file__).resolve().parent
CONFIGS = [
    ("constant_memory", "Constant memory (uncapped)"),
    ("96_registers", "96-register cap"),
    ("85_registers", "85-register cap"),
    ("80_registers", "80-register cap"),
    ("75_registers", "75-register cap"),
]
PREFIX = "640_360_FP32_64T_"


def timing_ms(path):
    values = re.findall(r"Average render time \(ms\):\s*([\d.]+)", path.read_text())
    if len(values) != 1:
        raise ValueError(f"Expected one timing in {path}")
    return float(values[0])


def read_metric(action, name):
    if name not in action.metric_names():
        raise ValueError(f"Missing metric {name!r}; reprofile with the roofline section")
    metric = action.metric_by_name(name)
    if not metric.has_value():
        raise ValueError(f"Metric {name!r} has no value")
    value = float(metric.value())
    if not np.isfinite(value) or value < 0:
        raise ValueError(f"Invalid value for {name}: {value}")
    return value


def fp32_work(action):
    """Count active, predicated-on thread operations; FMA is two FLOPs.

    These reports save per-cycle instruction rates rather than .sum counts.
    avg.per_cycle_elapsed * cycles_elapsed.sum reverses that normalization.
    Never multiply these thread-level counters by the warp width.
    """
    work = 0.0
    methods = []
    for op, weight in (("fadd", 1), ("fmul", 1), ("ffma", 2)):
        prefix = f"smsp__sass_thread_inst_executed_op_{op}_pred_on"
        if prefix + ".sum" in action.metric_names():
            count = read_metric(action, prefix + ".sum")
            methods.append(prefix + ".sum")
        else:
            rate = read_metric(action, prefix + ".avg.per_cycle_elapsed")
            cycles = read_metric(action, "smsp__cycles_elapsed.sum")
            count = rate * cycles
            methods.append(prefix + ".avg.per_cycle_elapsed * smsp__cycles_elapsed.sum")
        work += weight * count
    return work, "; ".join(methods)


def load_profiles(root=ROOT):
    try:
        import ncu_report
    except ImportError as exc:
        raise ImportError("Install NVIDIA's report reader with: pip install ncu-report") from exc
    rows = []
    for suffix, label in CONFIGS:
        stem = PREFIX + suffix
        source = Path("stats/profiles") / (stem + ".compute.ncu-rep")
        context = ncu_report.load_report(str(root / source))
        actions = []
        for range_index in range(context.num_ranges()):
            profile_range = context.range_by_idx(range_index)
            for action_index in range(profile_range.num_actions()):
                action = profile_range.action_by_idx(action_index)
                if "render_kernel" in action.name():
                    actions.append((range_index, action_index, action))
        if len(actions) != 1:
            raise ValueError(f"{source}: expected one render_kernel, found {len(actions)}")
        range_index, action_index, action = actions[0]
        get = lambda name: read_metric(action, name)
        duration_metric = action.metric_by_name("gpu__time_duration.avg")
        if duration_metric is None or duration_metric.unit() not in ("ns", "nsecond"):
            raise ValueError(f"{source}: expected duration in ns")
        duration = get("gpu__time_duration.avg") * 1e-9
        work, work_metrics = fp32_work(action)
        traffic = get("dram__bytes_read.sum") + get("dram__bytes_write.sum")
        clock = get("sm__cycles_elapsed.avg.per_second")
        compute_peak = 2 * get("sm__sass_thread_inst_executed_op_ffma_pred_on.sum.peak_sustained") * clock
        peak_bandwidth = get("dram__bytes.sum.peak_sustained") * get("dram__cycles_elapsed.avg.per_second")
        if min(duration, work, traffic, compute_peak, peak_bandwidth) <= 0:
            raise ValueError(f"{source}: nonpositive roofline input")
        performance = work / duration
        benchmark_path = Path("stats/reports") / (stem + ".timing.txt")
        benchmark_ms = timing_ms(root / benchmark_path)
        standalone_path = (Path("stats/reports/640_360_FP32_64T_final.timing.txt")
                           if suffix == "80_registers" else benchmark_path)
        own_roof = min(compute_peak, peak_bandwidth * work / traffic)
        device = action.metric_by_name("device__attribute_display_name").as_string()
        rows.append(dict(
            label=label, config=suffix, source=str(source),
            range_index=range_index, action_index=action_index, device=device,
            work_metrics=work_metrics,
            timing_source=str(benchmark_path), benchmark_ms=benchmark_ms,
            timing_context=("profiling-session timing (user clarified)" if suffix == "80_registers"
                            else "saved CUDA-event benchmark"),
            standalone_source=str(standalone_path),
            standalone_ms=timing_ms(root / standalone_path),
            profile_ms=duration * 1e3,
            sm_count=int(get("device__attribute_multiprocessor_count")),
            sm_clock_ghz=clock / 1e9,
            fp32_peak_tflops=compute_peak / 1e12,
            fp32_peak_pct=100 * performance / compute_peak,
            achieved_tflops=performance / 1e12,
            dram_gbps=traffic / duration / 1e9,
            dram_peak_gbps=peak_bandwidth / 1e9,
            dram_pct=100 * traffic / duration / peak_bandwidth,
            work_gflop=work / 1e9, traffic_mb=traffic / 1e6,
            intensity=work / traffic,
            occupancy_pct=get("sm__warps_active.avg.pct_of_peak_sustained_active"),
            roof_fraction_pct=100 * performance / own_roof,
            gap_to_roof=own_roof / performance,
        ))
    if len({r["device"] for r in rows}) != 1:
        raise ValueError("Profiles use different GPUs")
    return rows, median(r["dram_peak_gbps"] for r in rows)


def plot_roofline(rows, peak_gbps):
    peak = median(r["fp32_peak_tflops"] for r in rows)
    best = min(rows, key=lambda r: r["standalone_ms"])
    fig, ax = plt.subplots(figsize=(9, 5.5), layout="constrained")
    x = np.geomspace(0.1, 1e4, 300)
    ax.loglog(x, np.minimum(peak, peak_gbps * x / 1000), color="0.25",
              lw=2, label=f"Roof: {peak:.2f} TFLOP/s, {peak_gbps:.0f} GB/s")
    ax.axvline(peak * 1000 / peak_gbps, color="0.6", ls=":", lw=1)
    for i, row in enumerate(rows):
        winner = row is best
        ax.scatter(row["intensity"], row["achieved_tflops"], color=f"C{i}",
                   marker="*" if winner else "o", s=160 if winner else 65,
                   edgecolors="white", linewidths=0.6, zorder=3,
                   label=row["label"] + (" (best standalone)" if winner else ""))
    ax.set(xlim=(0.1, 1e4), ylim=(0.015, 15),
           xlabel="Arithmetic intensity (FP32 FLOP/byte)",
           ylabel="FP32 performance (TFLOP/s)",
           title="FP32 DRAM roofline · RTX 3050 Laptop · 64 threads/block")
    ax.grid(which="major", alpha=0.2)
    ax.legend(loc="upper left", fontsize=9, frameon=False)
    return fig


def write_outputs(rows, peak_gbps, output):
    output.mkdir(parents=True, exist_ok=True)
    with (output / "roofline_profiles.csv").open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    fig = plot_roofline(rows, peak_gbps)
    for ext in ("png", "svg"):
        fig.savefig(output / f"roofline_profiles.{ext}", dpi=180)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=ROOT / "stats/reports")
    args = parser.parse_args()
    rows, peak_gbps = load_profiles()
    write_outputs(rows, peak_gbps, args.output_dir)
    for row in rows:
        print(f"{row['label']:28s} I≈{row['intensity']:7.1f} FLOP/B  "
              f"P≈{row['achieved_tflops']:.3f} TFLOP/s  "
              f"Nsight kernel={row['profile_ms']:.3f} ms  standalone={row['standalone_ms']:.3f} ms")
    print(f"Saved CSV, PNG and SVG to {args.output_dir}")


if __name__ == "__main__":
    main()
