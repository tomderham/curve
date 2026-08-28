#!/usr/bin/env python3
"""
waveform_grid.py

Renders a side-by-side grid of solid amplitude-envelope blocks for
reference/captured pairs.

"""
import argparse
import contextlib
import io

import numpy as np

from compare_bitperfect import (align, decimate_minmax, find_integer_lag,
                                 load_mono, run_comparison)

PASS_COLOR = "#4caf50"
FAIL_COLOR = "#e53935"
REFERENCE_COLOR = "#4a7fc9"


def build_entry(label, reference_path, captured_path, hf_hz=20000.0, max_lag_seconds=2.0):
    with contextlib.redirect_stdout(io.StringIO()):
        metrics = run_comparison(reference_path, captured_path, hf_hz=hf_hz,
                                  max_lag_seconds=max_lag_seconds)

    sr_ref, ref = load_mono(reference_path)
    sr_cap, cap = load_mono(captured_path)
    lag = find_integer_lag(ref, cap, sr_ref, max_lag_seconds)
    ref_a, cap_a = align(ref, cap, lag)

    return {
        "label": label,
        "sr": sr_ref,
        "reference": ref_a,
        "captured": cap_a,
        "verdict": metrics.get("verdict", "UNKNOWN"),
        "gain": metrics.get("gain"),
    }


def save_waveform_grid_plot(entries, out_path, dark=True, target_points=3000,
                             width_per_entry=20.0, min_width=20.0):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    n = len(entries)
    ylim = max(
        (np.max(np.abs(e["captured"])) if e["captured"].size else 0.0)
        for e in entries
    )
    ylim = max(ylim, max(
        (np.max(np.abs(e["reference"])) if e["reference"].size else 0.0)
        for e in entries
    )) * 1.05

    fig_width = max(min_width, width_per_entry * n)
    fig, axes = plt.subplots(2, n, figsize=(fig_width, 5), squeeze=False)

    bg = "#1e1e1e" if dark else "white"
    fg = "white" if dark else "black"
    fig.patch.set_facecolor(bg)

    for col, entry in enumerate(entries):
        sr = entry["sr"]
        cap_color = PASS_COLOR if entry["verdict"] == "PASS" else FAIL_COLOR

        ax_cap = axes[0][col]
        idx, lo, hi = decimate_minmax(entry["captured"], target_points)
        ax_cap.fill_between(idx / sr, lo, hi, step="mid", color=cap_color, linewidth=0)
        ax_cap.set_ylim(-ylim, ylim)
        ax_cap.set_title(entry["label"], color=fg, fontsize=10)
        subtitle = entry.get("subtitle")
        if subtitle is None:
            gain = entry.get("gain")
            subtitle = (f"gain: {gain:.6f} ({20*np.log10(abs(gain)):+.3f} dB)"
                        if gain else "gain: n/a")
        ax_cap.text(0.02, 0.90, f"{entry['verdict']}\n{subtitle}", transform=ax_cap.transAxes,
                    color=fg, fontsize=8, va="top")

        ax_ref = axes[1][col]
        idx, lo, hi = decimate_minmax(entry["reference"], target_points)
        ax_ref.fill_between(idx / sr, lo, hi, step="mid", color=REFERENCE_COLOR, linewidth=0)
        ax_ref.set_ylim(-ylim, ylim)

        for ax in (ax_cap, ax_ref):
            ax.set_facecolor(bg)
            ax.set_xticks([])
            ax.set_yticks([])
            for spine in ax.spines.values():
                spine.set_color(fg)
                spine.set_alpha(0.3)

        if col == 0:
            ax_cap.set_ylabel("captured", color=fg, fontsize=9)
            ax_ref.set_ylabel("reference", color=fg, fontsize=9)

    fig.tight_layout()
    fig.savefig(out_path, dpi=150, facecolor=bg)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--entry", nargs=3, action="append", required=True,
                     metavar=("LABEL", "REFERENCE_WAV", "CAPTURED_WAV"),
                     help="one column: a label plus its reference and captured WAV paths "
                          "(repeat --entry for each app/condition to compare)")
    ap.add_argument("--hf-hz", type=float, default=20000.0)
    ap.add_argument("--max-lag-seconds", type=float, default=2.0)
    ap.add_argument("--out", required=True, help="output PNG path")
    ap.add_argument("--light", action="store_true", help="light background instead of dark")
    args = ap.parse_args()

    entries = []
    for label, reference_path, captured_path in args.entry:
        print(f"Processing '{label}'...")
        entries.append(build_entry(label, reference_path, captured_path,
                                    hf_hz=args.hf_hz, max_lag_seconds=args.max_lag_seconds))
        print(f"  verdict: {entries[-1]['verdict']}")

    save_waveform_grid_plot(entries, args.out, dark=not args.light)
    print(f"\nSaved grid to {args.out}")


if __name__ == "__main__":
    main()
