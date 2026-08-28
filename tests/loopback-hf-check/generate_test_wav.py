#!/usr/bin/env python3
"""
generate_test_wav.py

Generates exponential sine-sweep WAV test signals.

Usage:
    python3 generate_test_wav.py [--out-dir DIR] [--sweep-seconds S]
                                  [--amplitude A] [--nyquist-fraction F]
                                  [--formats float32 pcm24] [--f-lo HZ]

    --f-lo raises the sweep's start frequency (f_hi stays Nyquist-derived as
    always), concentrating more of the fixed sweep duration into the top of
    the band -- e.g. --f-lo 15000 spends ~75% of the sweep above 20 kHz at
    96 kHz, vs. ~11% for the default 20 Hz start.
"""
import argparse
import os

import numpy as np
import wave
from scipy.io import wavfile

PCM24_FULL_SCALE = 2 ** 23  # symmetric/Q23 convention -- see write_pcm24() below
PCM24_MAX = 2 ** 23 - 1  # 8388607, the largest representable positive code

DEFAULT_SWEEP_SECONDS = 10.0
DEFAULT_SILENCE_SECONDS = 1.0
DEFAULT_AMPLITUDE = 0.25
DEFAULT_NYQUIST_FRACTION = 0.98
DEFAULT_F_LO = 20.0


def exp_sweep(sample_rate, f1, f2, duration, amplitude=0.25):
    """Exponential sine sweep from f1 to f2 Hz over `duration` seconds -- no
    fade/window applied at either end.
"""
    n = int(duration * sample_rate)
    t = np.arange(n) / sample_rate
    L = duration / np.log(f2 / f1)
    K = f1 * L
    phase = 2 * np.pi * K * (np.exp(t / L) - 1.0)
    sweep = np.sin(phase)
    return (amplitude * sweep).astype(np.float64)


def write_float32(out_path, sample_rate, stereo_float):
    wavfile.write(out_path, sample_rate, stereo_float.astype(np.float32))


def write_pcm24(out_path, sample_rate, stereo_float):
    ints = np.clip(np.round(stereo_float * PCM24_FULL_SCALE), -PCM24_FULL_SCALE, PCM24_MAX).astype(np.int32)
    # Pack as little-endian 3-byte samples: view each int32 as 4 LE bytes
    # and drop the (redundant, sign-extension-only) top byte.
    as_int32_bytes = ints.flatten(order="C").astype("<i4").tobytes()
    as_bytes = np.frombuffer(as_int32_bytes, dtype=np.uint8).reshape(-1, 4)
    packed = as_bytes[:, :3].tobytes()

    n_channels = stereo_float.shape[1]
    with wave.open(out_path, "wb") as wf:
        wf.setnchannels(n_channels)
        wf.setsampwidth(3)
        wf.setframerate(sample_rate)
        wf.writeframes(packed)


WRITERS = {
    "float32": write_float32,
    "pcm24": write_pcm24,
}


def build_test_file(sample_rate, f_lo, f_hi, sweep_seconds, silence_seconds,
                     amplitude, out_path, fmt):
    sweep = exp_sweep(sample_rate, f_lo, f_hi, sweep_seconds, amplitude)
    silence = np.zeros(int(silence_seconds * sample_rate), dtype=np.float64)
    mono = np.concatenate([silence, sweep, silence])
    stereo = np.column_stack([mono, mono])

    WRITERS[fmt](out_path, sample_rate, stereo)

    print(f"Wrote {out_path}:")
    print(f"  format           : {fmt}")
    print(f"  sample rate      : {sample_rate} Hz")
    print(f"  sweep range      : {f_lo:.1f} Hz -> {f_hi:.1f} Hz (exponential)")
    print(f"  total duration   : {stereo.shape[0] / sample_rate:.3f} s "
          f"({stereo.shape[0]} samples/channel)")
    print(f"  peak amplitude   : {amplitude:.4f} ({20*np.log10(amplitude):.1f} dBFS)")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--sweep-seconds", type=float, default=DEFAULT_SWEEP_SECONDS,
                     help=f"duration of the swept portion (default {DEFAULT_SWEEP_SECONDS:g}s)")
    ap.add_argument("--silence-seconds", type=float, default=DEFAULT_SILENCE_SECONDS,
                     help=f"silence padding before/after the sweep (default {DEFAULT_SILENCE_SECONDS:g}s)")
    ap.add_argument("--amplitude", type=float, default=DEFAULT_AMPLITUDE,
                     help=f"linear peak amplitude, {DEFAULT_AMPLITUDE:g} = "
                          f"{20*np.log10(DEFAULT_AMPLITUDE):.0f} dBFS (default)")
    ap.add_argument("--nyquist-fraction", type=float, default=DEFAULT_NYQUIST_FRACTION,
                     help=f"sweep stops at this fraction of Nyquist (default {DEFAULT_NYQUIST_FRACTION:g})")
    ap.add_argument("--f-lo", type=float, default=None,
                     help="sweep start frequency in Hz (default 20). When "
                          "explicitly passed, the value is included in the "
                          "output filename (e.g. sweep_96000_pcm24_flo15000.wav) "
                          "so an HF-focused sweep can't silently overwrite the "
                          "standard sweep_<rate>_<format>.wav files the test "
                          "scripts reference by that exact name")
    ap.add_argument("--rates", type=int, nargs="+", default=[44100, 48000, 88200, 96000],
                     help="sample rates to generate files for "
                          "(default 44100 48000 88200 96000)")
    ap.add_argument("--formats", nargs="+", choices=sorted(WRITERS), default=["float32", "pcm24"],
                     help="WAV formats to generate (default: both float32 and pcm24)")
    ap.add_argument("--out-dir", default=".")
    args = ap.parse_args()

    os.makedirs(args.out_dir, exist_ok=True)

    f_lo = args.f_lo if args.f_lo is not None else DEFAULT_F_LO
    f_lo_tag = f"_flo{args.f_lo:g}" if args.f_lo is not None else ""

    for sr in args.rates:
        f_hi = args.nyquist_fraction * (sr / 2.0)
        for fmt in args.formats:
            out_path = os.path.join(args.out_dir, f"sweep_{sr}_{fmt}{f_lo_tag}.wav")
            build_test_file(sr, f_lo, f_hi, args.sweep_seconds,
                             args.silence_seconds, args.amplitude, out_path, fmt)
            print()


if __name__ == "__main__":
    main()
