"""Compare generated P6 images and optional planar FP32/FP16 framebuffers."""

import argparse
import math
import struct
import sys
from pathlib import Path


def read_ppm(path):
    with Path(path).open("rb") as image:
        if image.readline().strip() != b"P6":
            raise ValueError(f"{path}: expected a generated P6 image")
        width, height = map(int, image.readline().split())
        if image.readline().strip() != b"255":
            raise ValueError(f"{path}: expected 8-bit display channels")
        pixels = image.read()
    if len(pixels) != width * height * 3:
        raise ValueError(f"{path}: unexpected image size")
    return width, height, pixels


def read_linear(path, kind, count):
    data = Path(path).read_bytes()
    size = struct.calcsize(kind)
    if len(data) != count * size:
        raise ValueError(f"{path}: expected {count * size} bytes, got {len(data)}")
    # CUDA writes the host's native byte order; these comparisons run on that host.
    byte_order = "<" if sys.byteorder == "little" else ">"
    return [value for (value,) in struct.iter_unpack(byte_order + kind, data)]


def compare(reference_path, candidate_path, reference_linear=None, candidate_linear=None):
    width, height, reference = read_ppm(reference_path)
    other_width, other_height, candidate = read_ppm(candidate_path)
    if (width, height) != (other_width, other_height):
        raise ValueError("Image dimensions do not match")
    errors = [abs(a - b) for a, b in zip(reference, candidate)]
    mse = sum(error * error for error in errors) / len(errors)
    psnr = math.inf if mse == 0 else 10 * math.log10(255 * 255 / mse)
    changed = sum(any(errors[i:i + 3]) for i in range(0, len(errors), 3))
    print(f"Reference: {reference_path}")
    print(f"Candidate: {candidate_path}")
    print(f"Resolution: {width} x {height}")
    print(f"Display mean absolute channel error (0-255): {sum(errors) / len(errors):.6f}")
    print(f"Display maximum channel error (0-255): {max(errors)}")
    print(f"Display PSNR (dB): {psnr:.6f}")
    print(f"Changed pixels: {changed}/{width * height} ({100 * changed / (width * height):.3f}%)")
    if reference_linear is not None:
        baseline = read_linear(reference_linear, "f", len(reference))
        experiment = read_linear(candidate_linear, "e", len(reference))
        baseline_invalid = sum(not math.isfinite(value) for value in baseline)
        experiment_invalid = sum(not math.isfinite(value) for value in experiment)
        finite_errors = [abs(a - b) for a, b in zip(baseline, experiment)
                         if math.isfinite(a) and math.isfinite(b)]
        print(f"Reference non-finite linear channels: {baseline_invalid}")
        print(f"Candidate non-finite linear channels: {experiment_invalid}")
        if finite_errors:
            print(f"Linear MAE (finite pairs): {sum(finite_errors) / len(finite_errors):.9g}")
            print(f"Linear maximum absolute error (finite pairs): {max(finite_errors):.9g}")
        if baseline_invalid or experiment_invalid:
            print("INVALID LINEAR OUTPUT: display clamping can hide NaN/Inf values.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--reference-linear", type=Path)
    parser.add_argument("--candidate-linear", type=Path)
    args = parser.parse_args()
    if (args.reference_linear is None) != (args.candidate_linear is None):
        parser.error("Supply both linear framebuffers or neither")
    compare(args.reference, args.candidate, args.reference_linear, args.candidate_linear)
