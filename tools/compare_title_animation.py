#!/usr/bin/env python3
"""Measure repetition in two retained title clips, without running the game.

Requires ffmpeg, ffprobe, numpy and scipy. Calibration is specific to the two
SHA-256-verified files. It is not a general actor tracker or a frame-pacing test.
"""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
from scipy import ndimage, signal


FIXTURES = Path(__file__).resolve().parents[1] / "docs/evidence/title-animation-20261021"


def run(command):
    return subprocess.run(command, capture_output=True, check=True, timeout=60).stdout


def decode(path, calibration):
    if hashlib.sha256(path.read_bytes()).hexdigest() != calibration["sha256"]:
        raise ValueError(f"{path.name}: input hash differs from retained clip")
    metadata = json.loads(run([
        "ffprobe", "-v", "error", "-select_streams", "v:0", "-show_frames",
        "-show_entries", "frame=best_effort_timestamp_time", "-of", "json", str(path),
    ]))
    pts = np.array([float(f["best_effort_timestamp_time"]) for f in metadata["frames"]])
    fps = calibration["fps"]
    if len(pts) != calibration["frames"] or not np.allclose(
        pts - pts[0], np.arange(len(pts)) / fps, rtol=0, atol=0.000002
    ):
        raise ValueError(f"{path.name}: frame count/timestamps differ from calibration")
    raw = run([
        "ffmpeg", "-v", "error", "-i", str(path), "-vf", calibration["filters"],
        "-fps_mode", "passthrough", "-pix_fmt", "rgb24", "-f", "rawvideo", "-",
    ])
    shape = (calibration["frames"], calibration["height"], calibration["width"], 3)
    if len(raw) != int(np.prod(shape)):
        raise ValueError(f"{path.name}: decoded byte count differs from calibration")
    return np.frombuffer(raw, dtype=np.uint8).reshape(shape)


def mouth_signal(frames, calibration, red_ratio=1.55, blue_ratio=2.0):
    values = []
    for frame, region in zip(frames, calibration["regions_xyxy_scale2"]):
        x1, y1, x2, y2, scale2 = region
        if not (0 <= x1 < x2 <= frame.shape[1] and 0 <= y1 < y2 <= frame.shape[0] and scale2 > 0):
            raise ValueError("Invalid reviewed region")
        pixels = frame[y1:y2, x1:x2].astype(np.float64)
        red, green, blue = pixels[:, :, 0], pixels[:, :, 1], pixels[:, :, 2]
        # Suppress pale coat/background; sum the red mouth/lip component.
        weight = np.maximum(red - red_ratio * green, 0)
        weight *= np.clip(1 - blue_ratio * blue / np.maximum(red, 1), 0, 1)
        weight *= green > 25
        values.append(float(weight.sum()) / scale2)
    return np.array(values)


def period(values, fps=30, trim_start=0.5, trim_end=12.5):
    if not np.isfinite(values).all() or np.std(values) < 1e-12:
        raise ValueError("No usable variation")
    detrended = values - ndimage.gaussian_filter1d(values, fps)
    times = np.arange(len(values)) / fps
    selected = detrended[(times >= trim_start) & (times <= trim_end)]
    lags = np.arange(10, min(120, len(selected) // 2))
    if len(lags) < 10:
        raise ValueError("Insufficient duration")
    correlations = np.array([
        np.corrcoef(selected[:-lag], selected[lag:])[0, 1] for lag in lags
    ])
    peaks, _ = signal.find_peaks(correlations, prominence=0.25)
    candidates = [i for i in peaks if correlations[i] > 0.5]
    if not candidates:
        raise ValueError("No supported repetition peak")
    index = candidates[0]
    return {
        "period_seconds": float(lags[index] / fps),
        "autocorrelation": float(correlations[index]),
        "lag_frames": int(lags[index]),
        "resolution_seconds": 1 / fps,
    }


def analyze(frames, calibration):
    fps = calibration["fps"]
    values = mouth_signal(frames, calibration)
    measured = period(values, fps)
    measured["parameter_checks"] = []
    for red_ratio in (1.4, 1.55, 1.7, 1.85):
        for blue_ratio in (1.8, 2.0, 2.2):
            estimate = period(mouth_signal(frames, calibration, red_ratio, blue_ratio), fps)
            if abs(estimate["period_seconds"] - measured["period_seconds"]) > 1 / fps:
                raise AssertionError("Period is sensitive to colour parameters")
            measured["parameter_checks"].append({
                "red_ratio": red_ratio, "blue_ratio": blue_ratio, **estimate,
            })
    measured["resampled_signal_controls"] = []
    times = np.arange(len(values)) / fps
    for speed, expected in ((1.25, 1.2), (1.5, 1.0), (0.75, 2.0)):
        control_times = np.arange(0, times[-1] / speed, 1 / fps)
        control = np.interp(control_times * speed, times, values)
        estimate = period(control, fps, trim_end=min(12.5 / speed, control_times[-1] - 0.5))
        if abs(estimate["period_seconds"] - expected) > 1 / fps:
            raise AssertionError("Deliberate signal speed change was not detected")
        measured["resampled_signal_controls"].append({"speed": speed, **estimate})
    rejected = []
    for name, control in (
        ("constant", np.ones_like(values)),
        ("shuffled", np.random.default_rng(104).permutation(values)),
    ):
        try:
            period(control, fps)
        except ValueError:
            rejected.append(name)
        else:
            raise AssertionError(f"{name} negative control was accepted")
    measured["rejected_controls"] = rejected
    # Repeated mouth landmarks are cross-checks, not exact animation phase reads.
    detrended = values - ndimage.gaussian_filter1d(values, fps)
    smoothed = ndimage.gaussian_filter1d(detrended, 2)
    peaks, _ = signal.find_peaks(smoothed, distance=33, prominence=np.std(smoothed) * 0.5)
    measured["mouth_landmarks_seconds"] = [
        float(i / fps) for i in peaks if 1 <= i / fps <= 12.5
    ]
    measured["input_sha256"] = calibration["sha256"]
    measured["frames"] = calibration["frames"]
    measured["fps"] = fps
    return measured


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--odin", required=True, type=Path)
    parser.add_argument("--reference", required=True, type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    results = {"scope": "Klungo repeated title movement in these two clips only", "clips": {}}
    for name, path in (("odin", args.odin), ("reference", args.reference)):
        calibration = json.loads((FIXTURES / f"{name}-calibration.json").read_text())
        results["clips"][name] = analyze(decode(path, calibration), calibration)
    text = json.dumps(results, indent=2) + "\n"
    if args.output:
        args.output.write_text(text)
    print(text, end="")


if __name__ == "__main__":
    main()
