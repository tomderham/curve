#!/usr/bin/env python3
"""
run_external_capture_test.py

Tests bit-perfect nature of Curve's loopback functionality, where the output of Curve 
is captured by a separate sounddevice (PortAudio) application.

Usage:
    # list audio devices and their PortAudio indices
    python3 run_external_capture_test.py --list-devices

    # run the test
    python3 run_external_capture_test.py --rate 96000 --input-device X

    where X is replaced by the index of the Blackhole input device from previous step

"""
import argparse
import json
import os
import subprocess
import sys
import time

import numpy as np
from scipy.io import wavfile

from compare_bitperfect import check_notch, load_and_align, print_notch_report, run_comparison
from generate_test_wav import (DEFAULT_AMPLITUDE, DEFAULT_F_LO,
                                DEFAULT_NYQUIST_FRACTION, DEFAULT_SILENCE_SECONDS,
                                DEFAULT_SWEEP_SECONDS, build_test_file)
from waveform_grid import build_entry, save_waveform_grid_plot

HERE = os.path.dirname(os.path.abspath(__file__))

NOTCH_HZ = 1000.0
NOTCH_CUT_DB = 20.0
NOTCH_MIN_DETECT_DB = 10.0

FORMAT = "pcm24"
CHANNELS = 2
HF_HZ = 20000.0
RESULTS_DIR = os.path.join(HERE, "results_external")
PRIMING_SECONDS = 1.0
EXTRA_SETTLE_SECONDS = 1.5


def pause(msg):
    input(f"\n{msg}\nPress Enter to continue...")


def require_sounddevice():
    try:
        import sounddevice  # noqa: F401
        return sounddevice
    except ImportError:
        sys.exit("sounddevice not found. Install it with:\n"
                  "    pip3 install sounddevice\n"
                  "If `import sounddevice` still fails after that, also run:\n"
                  "    brew install portaudio")


def print_no_args_help():
    print(f"""
{'=' * 72}
Curve external output-capture test -- setup
{'=' * 72}

This test needs a few things installed first:

  brew install blackhole-2ch   
  pip3 install sounddevice     # PortAudio-based recording (if `import
                                # sounddevice` fails afterwards, also run
                                # `brew install portaudio`)

Once installed:

  1. Run this script with --list-devices to find BlackHole's index:
         python3 {os.path.basename(__file__)} --list-devices

  2. Run the actual test, e.g.:
         python3 {os.path.basename(__file__)} --rate 96000 \\
             --input-device <the index from step 1>
     This will first walk you through a quick test to confirm Curve really is in the signal chain,
     (by checking an EQ cut is in place), and then runs the real test.

""")


def list_devices():
    sd = require_sounddevice()
    print(sd.query_devices())
    print("\n--> Use the SAME BlackHole device for macOS system output, Curve's "
          "input AND output, and --input-device below.")

    try:
        devices = sd.query_devices()
        blackhole = [(i, d["name"]) for i, d in enumerate(devices)
                     if "blackhole" in d["name"].lower() and d["max_input_channels"] > 0]
        if len(blackhole) > 1:
            print(f"\nFound {len(blackhole)} BlackHole devices -- any of them works "
                  f"equally well (the specific product doesn't matter here, only using "
                  f"the same one everywhere does). Pick one:")
            for idx, name in blackhole:
                print(f"  \"{name}\" at index {idx} -- use:  --input-device {idx}")
        elif blackhole:
            idx, name = blackhole[0]
            print(f"\nFound \"{name}\" at index {idx} -- use:  --input-device {idx}")
        else:
            print("\nNo BlackHole input device found in the list above -- if you "
                  "installed it with `brew install blackhole-2ch`, try restarting "
                  "CoreAudio (`sudo killall coreaudiod`) or logging out/in, then run "
                  "--list-devices again.")
    except Exception:
        pass  # the raw query_devices() printout above is still useful on its own


def ensure_reference_wav(path, rate, f_lo):
    """Generates the reference sweep WAV on demand if it doesn't already exist,
    using generate_test_wav.py's own defaults (same function it calls itself)
    so this never has to be run as a separate manual step first."""
    if os.path.isfile(path):
        return
    print(f"\nReference file not found -- generating it now: {path}")
    f_hi = DEFAULT_NYQUIST_FRACTION * (rate / 2.0)
    build_test_file(rate, f_lo, f_hi, DEFAULT_SWEEP_SECONDS, DEFAULT_SILENCE_SECONDS,
                     DEFAULT_AMPLITUDE, path, FORMAT)
    print()


def sweep_duration_seconds(path):
    try:
        sr, data = wavfile.read(path)
        return len(data) / sr
    except Exception:
        return None


def record_fixed_duration(sd, input_device, samplerate, channels, duration_seconds):
    """Starts a non-blocking PortAudio recording of exactly duration_seconds
    at samplerate/channels, returning the (not-yet-filled) buffer array.
    Call sd.wait() once the caller's own timeline (priming + playback +
    settle) has elapsed to block until PortAudio has actually filled it."""
    frames = int(round(duration_seconds * samplerate))
    print(f"\nStarting external recording: {frames} frames @ {samplerate} Hz, "
          f"{channels}ch, device {input_device} ({duration_seconds:.1f}s)")
    return sd.rec(frames, samplerate=samplerate, channels=channels,
                  device=input_device, dtype="float32", blocking=False)


def record_capture(sd, input_device, rate, channels, reference, out_path):
    """Runs one full record -> afplay -> wait -> write-wav cycle. Shared by
    both the notch-validation pass and the real test, so both use
    the exact same recording mechanics."""
    dur = sweep_duration_seconds(reference) or 0.0
    safety_margin = 1.0
    total_recording_seconds = PRIMING_SECONDS + dur + EXTRA_SETTLE_SECONDS + safety_margin

    audio_buffer = record_fixed_duration(sd, input_device, rate, channels,
                                          total_recording_seconds)
    time.sleep(PRIMING_SECONDS)

    print(f"\nPlaying {os.path.basename(reference)} via afplay"
          + (f" (~{dur:.1f}s)..." if dur else "..."))
    result = subprocess.run(["afplay", reference])
    if result.returncode != 0:
        sd.stop()
        sys.exit(f"afplay exited with code {result.returncode} -- aborting.")
    print("Playback finished.")

    if EXTRA_SETTLE_SECONDS > 0:
        time.sleep(EXTRA_SETTLE_SECONDS)

    print("Waiting for the recording buffer to finish filling...")
    sd.wait()
    wavfile.write(out_path, rate, audio_buffer)
    print(f"Recording saved to {out_path}")

    actual_peak = float(np.max(np.abs(audio_buffer))) if audio_buffer.size else 0.0
    print(f"Captured peak level: {actual_peak:.4f} "
          f"({20*np.log10(actual_peak) if actual_peak > 0 else float('-inf'):.1f} dBFS)")
    return out_path


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--list-devices", action="store_true",
                     help="list PortAudio devices and exit")
    ap.add_argument("--rate", type=int, choices=[44100, 48000, 88200, 96000])
    ap.add_argument("--input-device", type=int,
                     help="sounddevice/PortAudio input device index (see --list-devices)")
    ap.add_argument("--f-lo", type=float, default=None,
                     help="use an HF-focused sweep starting at this frequency (Hz) as "
                          "the REAL test signal instead of the standard 20 Hz-start "
                          f"sweep, so more of the fixed sweep duration lands above "
                          f"{HF_HZ:.0f} Hz. Generated automatically on first use if it "
                          "doesn't exist yet -- no separate generate_test_wav.py step "
                          "needed. Only affects the real test -- the loopback-validation "
                          "pass always uses the standard full-range sweep, since the "
                          "1 kHz notch needs to stay in range.")
    ap.add_argument("--skip-setup-prompt", action="store_true")
    ap.add_argument("--selftest", action="store_true",
                     help="bypass Curve entirely and just check whether your loopback "
                          "device can capture audio at all (set System Settings -> "
                          "Sound -> Output to the loopback device first). Use this first "
                          "when chasing a bad recording, to find out whether the problem "
                          "is in Curve's config or the recording layer.")

    if len(sys.argv) == 1:
        print_no_args_help()
        return

    args = ap.parse_args()

    if args.list_devices:
        list_devices()
        return

    if args.rate is None:
        args.rate = 96000 if args.selftest else None
    if args.rate is None or args.input_device is None:
        sys.exit("--rate and --input-device are required (run --list-devices first)")

    sd = require_sounddevice()

    flo_tag = f"_flo{args.f_lo:g}" if args.f_lo is not None else ""

    # The notch-validation pass always uses the standard, full-range (20 Hz-start)
    # sweep regardless of --f-lo, since the 1 kHz notch needs to stay in range
    notch_reference = os.path.join(HERE, f"sweep_{args.rate}_{FORMAT}.wav")
    reference = os.path.join(HERE, f"sweep_{args.rate}_{FORMAT}{flo_tag}.wav")

    ensure_reference_wav(notch_reference, args.rate, DEFAULT_F_LO)
    ensure_reference_wav(reference, args.rate, args.f_lo if args.f_lo is not None else DEFAULT_F_LO)

    print("=" * 72)
    print(f"Curve EXTERNAL output-capture test -- {args.rate} Hz, {FORMAT} source"
          + (f", HF-focused sweep (f_lo={args.f_lo:g} Hz)" if args.f_lo is not None else ""))
    print("=" * 72)

    if args.selftest:
        print("""
SELF-TEST mode: this bypasses Curve entirely, to check whether your
loopback device can capture audio at all before blaming Curve for a bad
recording.

  1. In macOS sound controls, set macOS system output interface to your loopback
     device (e.g. "BlackHole 2ch").
  2. That's it -- afplay will play straight to that device and this script
     records it back, with no Curve involvement whatsoever.

If this comes back PASS, the recording layer is fine and the problem is
specifically in how Curve is configured (wrong output device, wrong graph wiring, etc). If THIS
also comes back bad, the problem is upstream of Curve entirely.
""")
        pause("Once System Settings -> Sound -> Output is set as above, come back here.")
    elif not args.skip_setup_prompt:
        print(f"""
Manual setup

  1. In Audio MIDI Setup, set your BlackHole device's nominal rate to
     {args.rate} Hz.

  2. In Curve:
       - Audio Settings: input AND output device = that same BlackHole
         device.
       - Build the graph:
             Interface Loopback (In) -> AUNBandEQ -> Audio Output
         (no other nodes)

  3. In macOS sound controls, set macOS system output interface to the loopback device (e.g. "BlackHole 2ch").

  4. --input-device below should be that same BlackHole device's index.

To ensure that everything is configured correctly and Curve's loopback really is in the 
signal chain, the script will walk you through a quick loopback
validation before running the real test.

If you're chasing a bad recording, try `--selftest` first: it isolates
whether your loopback device can capture anything at all, with no Curve
involvement, so you know which layer to keep debugging.
""")
        pause("Once Curve is configured as above, come back here.")

    os.makedirs(RESULTS_DIR, exist_ok=True)
    stamp = time.strftime("%Y-%m-%d_%H%M%S")

    if not args.selftest:
        print(f"""
{'=' * 72}
LOOPBACK VALIDATION -- confirms Curve is genuinely in the signal path
before trusting the real test below.
{'=' * 72}

  1. In Curve, open the AUNBandEQ node you inserted, and configure it with a single band, with
 -{NOTCH_CUT_DB:.0f} dB gain at {NOTCH_HZ:.0f} Hz with width of 1.
  2. Make sure the EQ is enabled (not bypassed).

""")
        pause("Once the EQ notch is inserted and enabled, come back here.")

        validate_dir = os.path.join(RESULTS_DIR, f"validate_{stamp}")
        os.makedirs(validate_dir, exist_ok=True)
        validate_capture_path = os.path.join(validate_dir, "validation_capture.wav")

        record_capture(sd, args.input_device, args.rate, CHANNELS, notch_reference,
                        validate_capture_path)

        aligned = load_and_align(notch_reference, validate_capture_path)
        if aligned is None:
            sys.exit("Sample-rate mismatch between reference and validation capture -- "
                      "check your loopback device's nominal rate matches --rate.")
        sr_v, ref_a, cap_a, _lag = aligned
        print()
        notch_report = check_notch(ref_a, cap_a, sr_v, NOTCH_HZ,
                                    min_depth_db=NOTCH_MIN_DETECT_DB)
        print_notch_report(notch_report)

        with open(os.path.join(validate_dir, "notch_check.json"), "w") as f:
            json.dump(notch_report, f, indent=2)

        validate_label = f"Validation -- {NOTCH_CUT_DB:.0f} dB cut @ {NOTCH_HZ:.0f} Hz"
        validate_entry = {
            "label": validate_label,
            "sr": sr_v,
            "reference": ref_a,
            "captured": cap_a,
            "verdict": "PASS" if notch_report["notch_found"] else "FAIL",
            "subtitle": (f"notch: {notch_report['notch_depth_db']:+.1f} dB "
                         f"(need >= {NOTCH_MIN_DETECT_DB:.0f} dB)"),
        }
        validate_waveform_path = os.path.join(validate_dir, "waveform.png")
        save_waveform_grid_plot([validate_entry], validate_waveform_path)
        print(f"Waveform plot saved to {validate_waveform_path} -- since the sweep "
              f"passes through {NOTCH_HZ:.0f} Hz at a specific point in time, the "
              f"{NOTCH_CUT_DB:.0f} dB cut should show up as a visible dip in "
              f"the captured envelope there.")

        if not notch_report["notch_found"]:
            sys.exit(f"\nStopping here -- fix the routing/EQ before running the real "
                      f"test (see {validate_dir}/notch_check.json for the measured "
                      f"values). Running the real test now would risk a meaningless "
                      f"PASS that doesn't actually reflect Curve's processing.")

        print(f"""
Validation passed -- Curve is genuinely in the signal path for this
routing configuration.

Now right click on the AUNBandEQ node in Curve, and select Toggle Bypass so it is greyed out and
bypassed (audio passes straight through it with no processing).
""")
        pause("Once the EQ is disabled/removed, come back here to run the real test.")

    prefix = "selftest" if args.selftest else f"{args.rate}Hz_{FORMAT}{flo_tag}"
    run_dir = os.path.join(RESULTS_DIR, f"{prefix}_{stamp}")
    os.makedirs(run_dir, exist_ok=True)
    captured_path = os.path.join(run_dir, "external_capture.wav")

    record_capture(sd, args.input_device, args.rate, CHANNELS, reference, captured_path)

    plot_path = os.path.join(run_dir, "diff_spectrum.png")
    waveform_plot_path = os.path.join(run_dir, "diff_waveform.png")
    print(f"\nRunning comparison...\n")
    metrics = run_comparison(reference, captured_path, hf_hz=HF_HZ, plot=plot_path,
                              waveform_plot=waveform_plot_path)

    with open(os.path.join(run_dir, "summary.json"), "w") as f:
        json.dump(metrics, f, indent=2)

    verdict = metrics.get("verdict")
    print(f"\n{'=' * 72}")
    print(f"VERDICT: {verdict}")
    print(f"Results saved under: {run_dir}")

    if verdict in ("CAPTURED_SILENCE", "NO_CORRELATION"):
        troubleshoot_target = "the recording layer" if args.selftest else "Curve's configuration"
        bullets = [
            "- Confirm --input-device is really your loopback device's index (run "
            "--list-devices again -- indices can shift between runs).",
            "- Confirm System Settings -> Sound -> Output (for --selftest) or Curve's "
            "Audio Settings output device (otherwise) is really set to that same "
            "loopback device right now.",
            "- Confirm your loopback device's nominal rate (Audio MIDI Setup) matches "
            "--rate exactly.",
            "- Confirm Curve's OUTPUT device specifically is set to your loopback "
            "device (Curve's Input device setting is not used by the tap or by "
            "\"Force macOS System Audio to loopback\" at all -- only Output matters).",
        ]
        if not args.selftest:
            bullets.insert(0, "- Run with --selftest to check whether the recording "
                              "layer works at all with Curve out of the picture.")
        captured_what = "anything" if args.selftest else "Curve's output"
        print(f"\nThis means the recording didn't actually capture {captured_what} -- "
              f"it is NOT a claim about filtering one way or the other. Troubleshoot "
              f"{troubleshoot_target} before re-running:")
        for b in bullets:
            print(f"  {b}")
        print()
    else:
        label = ("Self-test (no Curve)" if args.selftest
                  else f"Curve (external capture) -- {args.rate} Hz, {FORMAT}{flo_tag}")
        block_entry = build_entry(label, reference, captured_path, hf_hz=HF_HZ)
        save_waveform_grid_plot([block_entry], os.path.join(run_dir, "waveform.png"))

    print("=" * 72)


if __name__ == "__main__":
    main()
