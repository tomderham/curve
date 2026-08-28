#!/usr/bin/env python3
"""
run_loopback_capture_test.py

Tests bit-perfect nature of Curve's loopback functionality, where an audio file is played by a
reference player application, and the captured loopback audio in Curve is recorded by Curve's own audio recorder.

Usage:
    python3 run_loopback_capture_test.py --rate 96000

"""
import argparse
import glob
import json
import os
import shutil
import subprocess
import sys
import time

from scipy.io import wavfile

from compare_bitperfect import run_comparison
from waveform_grid import build_entry, save_waveform_grid_plot

HERE = os.path.dirname(os.path.abspath(__file__))
DESKTOP = os.path.expanduser("~/Desktop")


def pause(msg):
    input(f"\n{msg}\nPress Enter to continue...")


def find_captured_file(rate, not_before):
    pattern = os.path.join(DESKTOP, f"Curve_Capture_{rate}Hz_*.wav")
    all_candidates = sorted(glob.glob(pattern), key=os.path.getmtime)
    fresh = [f for f in all_candidates if os.path.getmtime(f) >= not_before - 1.0]
    return (fresh[-1] if fresh else None), all_candidates


def sweep_duration_seconds(path):
    try:
        sr, data = wavfile.read(path)
        return len(data) / sr
    except Exception:
        return None


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--rate", type=int, choices=[48000, 96000], required=True,
                     help="interface sample rate to test")
    ap.add_argument("--format", choices=["float32", "pcm24"], default="pcm24",
                     help="reference file format to play (default pcm24, the "
                          "typical DAW bounce format; float32 matches Curve's own "
                          "recorder bit depth exactly)")
    ap.add_argument("--hf-hz", type=float, default=20000.0)
    ap.add_argument("--exact-tolerance-lsb24", type=float, default=0.5,
                     help="direct-diff tolerance in fractions of a 24-bit LSB (default 0.5); "
                          "see compare_bitperfect.py --help")
    ap.add_argument("--results-dir", default=os.path.join(HERE, "results"))
    ap.add_argument("--extra-settle-seconds", type=float, default=1.5,
                     help="pause after afplay finishes, before asking you to stop "
                          "recording, so Curve's writer thread has drained (default 1.5s)")
    ap.add_argument("--skip-setup-prompt", action="store_true",
                     help="skip the initial setup instructions/pause (use once you've "
                          "already got Curve set up and are re-running the same rate)")
    args = ap.parse_args()

    reference = os.path.join(HERE, f"sweep_{args.rate}_{args.format}.wav")
    if not os.path.isfile(reference):
        sys.exit(f"Reference file not found: {reference}\n"
                  f"Run generate_test_wav.py first (or --rates {args.rate} "
                  f"--formats {args.format}).")

    print("=" * 72)
    print(f"Curve loopback HF test -- {args.rate} Hz, {args.format} source")
    print("=" * 72)

    if not args.skip_setup_prompt:
        print(f"""
Manual setup:

  1. In Audio MIDI Setup, set your target output interface's nominal
     sample rate to {args.rate} Hz.

  2. In Curve:
       - Audio Settings: output device = that interface; confirm the
         active sample rate shown is {args.rate} Hz.
       - Build the graph:
             Interface Loopback (In) -> Audio Recorder
             Interface Loopback (In) -> Audio Output (optional - so you can hear the audio)
         Do not insert any other node

  3. In macOS sound controls, set macOS system output interface to the loopback device (e.g. "BlackHole 2ch").

  4. Click "Start Recording" on the Audio Recorder node.
""")
    pause("Once recording is running in Curve, come back here.")

    start_time = time.time()

    dur = sweep_duration_seconds(reference)
    print(f"\nPlaying {os.path.basename(reference)} via afplay"
          + (f" (~{dur:.1f}s)..." if dur else "..."))
    result = subprocess.run(["afplay", reference])
    if result.returncode != 0:
        sys.exit(f"afplay exited with code {result.returncode} -- aborting.")
    print("Playback finished.")

    if args.extra_settle_seconds > 0:
        time.sleep(args.extra_settle_seconds)

    pause('Click "Stop Recording" on the Audio Recorder node in Curve now.')

    print(f"\nLooking for Curve_Capture_{args.rate}Hz_*.wav on {DESKTOP} ...")
    captured, all_candidates = find_captured_file(args.rate, start_time)
    if captured is None:
        print(f"\nNo matching file created after this run started was found.")
        if all_candidates:
            print("Found these existing (older) files instead -- pass one to "
                  "compare_bitperfect.py directly if one of these is actually correct:")
            for c in all_candidates:
                print(f"  {c}  (modified {time.ctime(os.path.getmtime(c))})")
        else:
            print("No Curve_Capture_*.wav files found on the Desktop at all -- check "
                  "that recording actually started/stopped, and that the sample rate "
                  "in Curve matches --rate.")
        sys.exit(1)

    print(f"Found: {captured}")

    os.makedirs(args.results_dir, exist_ok=True)
    stamp = time.strftime("%Y-%m-%d_%H%M%S", time.localtime(start_time))
    run_dir = os.path.join(args.results_dir, f"{args.rate}Hz_{args.format}_{stamp}")
    os.makedirs(run_dir, exist_ok=True)

    captured_copy = os.path.join(run_dir, os.path.basename(captured))
    shutil.copy2(captured, captured_copy)
    plot_path = os.path.join(run_dir, "diff_spectrum.png")
    waveform_plot_path = os.path.join(run_dir, "diff_waveform.png")

    print(f"\nRunning comparison (a copy of the capture, plots, and summary will be "
          f"saved to {run_dir})...\n")
    metrics = run_comparison(reference, captured_copy, hf_hz=args.hf_hz, plot=plot_path,
                              waveform_plot=waveform_plot_path,
                              exact_tolerance_lsb24=args.exact_tolerance_lsb24)

    summary_path = os.path.join(run_dir, "summary.json")
    with open(summary_path, "w") as f:
        json.dump(metrics, f, indent=2)

    block_label = f"Curve -- {args.rate} Hz, {args.format}"
    block_entry = build_entry(block_label, reference, captured_copy, hf_hz=args.hf_hz)
    block_path = os.path.join(run_dir, "waveform.png")
    save_waveform_grid_plot([block_entry], block_path)
    print(f"Saved DAW-style block view to {block_path}")

    print(f"\n{'=' * 72}")
    print(f"VERDICT: {metrics.get('verdict')}")
    print(f"Results saved under: {run_dir}")
    print(f"(original recording left in place on the Desktop: {captured})")
    print("=" * 72)


if __name__ == "__main__":
    main()
