#!/usr/bin/env python3
"""
compare_bitperfect.py

Bit-exact validation between a reference WAV and a test WAV.

Handles an unknown integer-sample offset at the start of the recording
    (found by cross-correlation)

Two following checks are then run on the aligned pair:
  1. A direct, sample-by-sample diff -- no gain correction, no averaging.
     Reports exactly how many samples are identical (to within a tolerance
     you set in fractions of an LSB at 24-bit, to absorb harmless int<->float
     conversion rounding), how many differ, the size of the biggest
     difference in dBFS *and* in LSBs at 16-bit/24-bit), and where the first and last differing sample are.
  2. A gain-corrected null test -- fits an overall linear gain by least
     squares first, then reports the residual's peak/RMS level in dBFS, and
     how much of the residual's energy sits at/above a configurable
     threshold (default 20 kHz) vs. how much of the *reference* signal's own
     energy is up there.

Also produces:
  - a Welch PSD comparison of reference vs. captured, and their dB
    difference, both printed at a handful of points and optionally plotted to a PNG (--plot)
  - an optional time-domain waveform plot (--waveform-plot): the aligned
    reference and captured waveforms overlaid, the raw diff, and a zoomed-in
    view around the single worst-diff sample

Usage:
    python3 compare_bitperfect.py reference.wav captured.wav \
        [--hf-hz 20000] [--max-lag-seconds 2.0] \
        [--plot spectrum.png] [--waveform-plot waveform.png]
"""
import argparse

import numpy as np
from scipy.io import wavfile
from scipy.signal import fftconvolve, welch

LSB_16BIT = 1.0 / (2 ** 15)
LSB_24BIT = 1.0 / (2 ** 23)


def load_mono(path):
    sr, data = wavfile.read(path)
    if np.issubdtype(data.dtype, np.integer):
        full_scale = 2 ** (np.iinfo(data.dtype).bits - 1)
        data = data.astype(np.float64) / full_scale
    else:
        data = data.astype(np.float64)
    if data.ndim > 1:
        data = data.mean(axis=1)
    return sr, data


def find_integer_lag(ref, cap, sr, max_lag_seconds):
    """Returns lag such that cap[n] corresponds to ref[n - lag]."""
    max_lag = int(max_lag_seconds * sr)
    corr = fftconvolve(cap, ref[::-1], mode="full")
    zero_lag_index = len(ref) - 1
    lo = max(0, zero_lag_index - max_lag)
    hi = min(len(corr), zero_lag_index + max_lag)
    window = corr[lo:hi]
    best = lo + int(np.argmax(np.abs(window)))
    return best - zero_lag_index


def align(ref, cap, lag):
    if lag >= 0:
        cap_a, ref_a = cap[lag:], ref
    else:
        ref_a, cap_a = ref[-lag:], cap
    n = min(len(ref_a), len(cap_a))
    return ref_a[:n], cap_a[:n]


def load_and_align(reference_path, captured_path, max_lag_seconds=2.0):
    sr_ref, ref = load_mono(reference_path)
    sr_cap, cap = load_mono(captured_path)
    if sr_ref != sr_cap:
        return None
    sr = sr_ref
    lag = find_integer_lag(ref, cap, sr, max_lag_seconds)
    ref_a, cap_a = align(ref, cap, lag)
    return sr, ref_a, cap_a, lag


def dbfs(x):
    x = np.asarray(x)
    peak = np.max(np.abs(x)) if x.size else 0.0
    rms = np.sqrt(np.mean(x ** 2)) if x.size else 0.0
    to_db = lambda v: 20 * np.log10(v) if v > 0 else -np.inf
    return to_db(peak), to_db(rms)


def hf_energy_fraction(sig, sr, hf_hz):
    spec = np.fft.rfft(sig)
    freqs = np.fft.rfftfreq(len(sig), d=1.0 / sr)
    total = np.sum(np.abs(spec) ** 2)
    hf = np.sum(np.abs(spec[freqs >= hf_hz]) ** 2)
    return (hf / total) if total > 0 else 0.0


def check_notch(ref_a, cap_a, sr, notch_hz, min_depth_db=6.0, control_hz=None,
                 control_tolerance_db=3.0):
    nperseg = min(65536, len(ref_a))
    f, p_ref = welch(ref_a, fs=sr, nperseg=nperseg)
    _, p_cap = welch(cap_a, fs=sr, nperseg=nperseg)
    with np.errstate(divide="ignore"):
        db_ref = 10 * np.log10(p_ref + 1e-24)
        db_cap = 10 * np.log10(p_cap + 1e-24)
    diff_db = db_cap - db_ref

    def diff_at(freq):
        idx = int(np.argmin(np.abs(f - freq)))
        return float(diff_db[idx]), float(f[idx])

    notch_diff_db, notch_measured_hz = diff_at(notch_hz)
    notch_depth_db = -notch_diff_db  # positive = attenuation, as expected for a cut

    if control_hz is None:
        control_hz = [max(50.0, notch_hz / 4.0), min(sr / 2.0 * 0.9, notch_hz * 4.0)]
    control_points = [diff_at(c) for c in control_hz]

    notch_found = (notch_depth_db >= min_depth_db
                   and all(abs(d) <= control_tolerance_db for d, _ in control_points))

    return {
        "notch_hz_requested": notch_hz,
        "notch_hz_measured_bin": notch_measured_hz,
        "notch_depth_db": notch_depth_db,
        "min_depth_db_required": min_depth_db,
        "control_points_hz": [c for _, c in control_points],
        "control_points_diff_db": [d for d, _ in control_points],
        "control_tolerance_db": control_tolerance_db,
        "notch_found": notch_found,
    }


def print_notch_report(report):
    print(f"Notch check at {report['notch_hz_requested']:.0f} Hz "
          f"(measured at nearest bin {report['notch_hz_measured_bin']:.1f} Hz): "
          f"{report['notch_depth_db']:+.1f} dB attenuation "
          f"(need >= {report['min_depth_db_required']:.1f} dB)")
    for hz, d in zip(report["control_points_hz"], report["control_points_diff_db"]):
        print(f"  Control point {hz:8.1f} Hz: {d:+6.2f} dB "
              f"(need within +/-{report['control_tolerance_db']:.1f} dB)")
    if report["notch_found"]:
        print("=> NOTCH DETECTED: the captured signal shows the expected cut. "
              "Curve's processing is genuinely in the signal path for this run.")
    else:
        print("=> NOTCH NOT DETECTED: the captured signal does not show the expected "
              "cut. Either the EQ node wasn't actually inserted/enabled between "
              "Interface Loopback (In) and Audio Output, or Curve's processing is "
              "being bypassed (e.g. via the loopback device's own native "
              "pass-through) -- don't trust a PASS from this test setup until this "
              "check succeeds.")


def direct_diff_report(ref_a, cap_a, sr, exact_tolerance_lsb24=0.5):
    diff = cap_a - ref_a
    n = len(diff)
    tolerance = exact_tolerance_lsb24 * LSB_24BIT

    literally_zero = int(np.count_nonzero(diff == 0.0))
    within_tolerance = int(np.count_nonzero(np.abs(diff) <= tolerance))
    differing = np.where(np.abs(diff) > tolerance)[0]

    peak_db, rms_db = dbfs(diff)
    max_abs = float(np.max(np.abs(diff))) if n else 0.0

    report = {
        "n_samples": n,
        "literally_identical_samples": literally_zero,
        "literally_identical_fraction": (literally_zero / n) if n else 1.0,
        "within_tolerance_samples": within_tolerance,
        "within_tolerance_fraction": (within_tolerance / n) if n else 1.0,
        "exact_tolerance_lsb24": exact_tolerance_lsb24,
        "raw_diff_peak_dbfs": peak_db,
        "raw_diff_rms_dbfs": rms_db,
        "max_diff_linear": max_abs,
        "max_diff_lsb16": max_abs / LSB_16BIT,
        "max_diff_lsb24": max_abs / LSB_24BIT,
        "first_differing_sample": int(differing[0]) if differing.size else None,
        "last_differing_sample": int(differing[-1]) if differing.size else None,
        "peak_diff_sample": int(np.argmax(np.abs(diff))) if n else None,
    }
    if report["first_differing_sample"] is not None:
        report["first_differing_time_s"] = report["first_differing_sample"] / sr
        report["last_differing_time_s"] = report["last_differing_sample"] / sr
    return diff, report


def print_direct_diff_report(report):
    n = report["n_samples"]
    print(f"Literally bit-identical samples: {report['literally_identical_samples']}/{n} "
          f"({report['literally_identical_fraction']*100:.4f}%)")
    print(f"Within {report['exact_tolerance_lsb24']:g} LSB (24-bit) tolerance: "
          f"{report['within_tolerance_samples']}/{n} "
          f"({report['within_tolerance_fraction']*100:.4f}%)")
    print(f"Largest raw diff: {report['raw_diff_peak_dbfs']:.1f} dBFS "
          f"({report['max_diff_lsb24']:.3f} LSB @ 24-bit, "
          f"{report['max_diff_lsb16']:.6f} LSB @ 16-bit)")
    if report["first_differing_sample"] is not None:
        print(f"First differing sample: index {report['first_differing_sample']} "
              f"({report['first_differing_time_s']:.4f}s)")
        print(f"Last differing sample:  index {report['last_differing_sample']} "
              f"({report['last_differing_time_s']:.4f}s)")
        print(f"Single worst sample:    index {report['peak_diff_sample']}")
    else:
        print("No samples exceeded the tolerance -- literal bit-for-bit match.")


def decimate_minmax(x, target_points):
    """Downsamples x to ~target_points by taking the min/max envelope of each
    bucket (the way DAW waveform displays render long files), so an overview
    plot of a multi-second, 96 kHz file stays fast and still shows every
    peak/dropout instead of aliasing into a solid blob or dropping transients."""
    n = len(x)
    if n <= target_points * 2:
        idx = np.arange(n)
        return idx, x, x
    bucket = int(np.ceil(n / target_points))
    n_buckets = int(np.ceil(n / bucket))
    pad = n_buckets * bucket - n
    xp = np.pad(x, (0, pad), mode="edge") if pad > 0 else x
    reshaped = xp.reshape(n_buckets, bucket)
    mins = reshaped.min(axis=1)
    maxs = reshaped.max(axis=1)
    idx = np.arange(n_buckets) * bucket + bucket / 2.0
    return idx, mins, maxs


def save_waveform_plot(ref_a, cap_a, diff, sr, path, gain=None, residual=None,
                        zoom_center_sample=None, zoom_ms=50.0):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    n = len(cap_a)
    t_full = np.arange(n) / sr
    has_residual = residual is not None

    fig, axes = plt.subplots(4 if has_residual else 3, 1, figsize=(11, 12 if has_residual else 9))
    ax1, ax2, ax3 = axes[0], axes[1], axes[2]
    ax4 = axes[3] if has_residual else None

    # Panel 1: full-duration overview, reference vs. captured (min/max envelope)
    idx_r, r_lo, r_hi = decimate_minmax(ref_a, 4000)
    idx_c, c_lo, c_hi = decimate_minmax(cap_a, 4000)
    ax1.fill_between(idx_r / sr, r_lo, r_hi, step="mid", alpha=0.5, label="reference")
    ax1.fill_between(idx_c / sr, c_lo, c_hi, step="mid", alpha=0.5, label="captured")
    ax1.set_ylabel("Amplitude")
    ax1.set_title("Aligned waveforms (full duration, min/max envelope)")
    ax1.legend(loc="upper right")

    # Panel 2: full-duration raw diff (no gain correction)
    idx_d, d_lo, d_hi = decimate_minmax(diff, 4000)
    ax2.fill_between(idx_d / sr, d_lo, d_hi, step="mid", color="tab:red", alpha=0.7)
    ax2.set_ylabel("captured - reference")
    title = "Raw diff (no gain correction)"
    if gain is not None:
        gain_db = 20 * np.log10(abs(gain)) if gain != 0 else float("-inf")
        title += f"  --  fitted gain: {gain:.9f} ({gain_db:+.6f} dB)"
    ax2.set_title(title)

    # Panel 3: zoomed waveform overlay at full sample resolution, centered on
    # the worst point in the gain-corrected residual if available
    zoom_label = "worst residual sample" if zoom_center_sample is not None else "midpoint"
    center = zoom_center_sample if zoom_center_sample is not None else n // 2
    half_w = int(sr * zoom_ms / 1000.0 / 2)
    lo = max(0, center - half_w)
    hi = min(n, center + half_w)
    tz = t_full[lo:hi] * 1000.0  # ms
    ax3.plot(tz, ref_a[lo:hi], label="reference", linewidth=1.0)
    ax3.plot(tz, cap_a[lo:hi], label="captured", linewidth=1.0, alpha=0.8)
    ax3.set_xlabel("Time (ms)")
    ax3.set_ylabel("Amplitude")
    ax3.set_title(f"Zoomed waveforms around {zoom_label} "
                   f"({zoom_ms:.0f} ms window, full sample resolution)")
    ax3.legend(loc="upper right")

    # Panel 4: the same zoomed window, but showing the *gain-corrected*
    # residual itself, auto-scaled to its own magnitude
    if has_residual:
        seg = residual[lo:hi]
        ax4.plot(tz, seg, linewidth=1.0, color="tab:purple")
        seg_peak = np.max(np.abs(seg)) if seg.size else 0.0
        pad = seg_peak * 0.15 if seg_peak > 0 else 1e-12
        ax4.set_ylim(-seg_peak - pad, seg_peak + pad)
        ax4.set_xlabel("Time (ms)")
        ax4.set_ylabel("Residual (gain removed)")
        ax4.set_title(f"Gain-corrected residual, same window, auto-scaled "
                       f"(peak here: {20*np.log10(seg_peak) if seg_peak > 0 else float('-inf'):.1f} dBFS)")

    fig.tight_layout()
    fig.savefig(path, dpi=150)


def run_comparison(reference_path, captured_path, hf_hz=20000.0, max_lag_seconds=2.0, plot=None,
                    waveform_plot=None, exact_tolerance_lsb24=0.5):
    sr_ref, ref = load_mono(reference_path)
    sr_cap, cap = load_mono(captured_path)

    print(f"Reference: {reference_path}  ({sr_ref} Hz, {len(ref)} samples, "
          f"{len(ref)/sr_ref:.3f}s)")
    print(f"Captured : {captured_path}  ({sr_cap} Hz, {len(cap)} samples, "
          f"{len(cap)/sr_cap:.3f}s)")

    result = {
        "reference": reference_path,
        "captured": captured_path,
        "sample_rate_reference": sr_ref,
        "sample_rate_captured": sr_cap,
    }

    if sr_ref != sr_cap:
        print(f"\n*** WARNING: sample-rate mismatch (reference={sr_ref} Hz, "
              f"captured={sr_cap} Hz). ***")
        print("This alone is a plausible root cause of an HF difference -- fix the "
              "interface/session rate before drawing any conclusion about filtering.\n")
        result["verdict"] = "SAMPLE_RATE_MISMATCH"
        return result

    sr = sr_ref

    lag = find_integer_lag(ref, cap, sr, max_lag_seconds)
    print(f"\nEstimated integer sample offset (captured relative to reference): "
          f"{lag} samples ({lag / sr * 1000:.3f} ms)")
    result["lag_samples"] = lag

    ref_a, cap_a = align(ref, cap, lag)

    # --- 0. Signal-presence / correlation-strength sanity check -------------
    cap_peak = float(np.max(np.abs(cap_a))) if len(cap_a) else 0.0
    ref_norm = float(np.sqrt(np.dot(ref_a, ref_a)))
    cap_norm = float(np.sqrt(np.dot(cap_a, cap_a)))
    corr_coeff = (float(np.dot(ref_a, cap_a)) / (ref_norm * cap_norm)
                  if ref_norm > 0 and cap_norm > 0 else 0.0)
    result["captured_peak_linear"] = cap_peak
    result["correlation_coefficient"] = corr_coeff

    SILENCE_THRESHOLD = 1e-4  # -80 dBFS
    CORRELATION_THRESHOLD = 0.3
    if cap_peak < SILENCE_THRESHOLD:
        print(f"\n*** Captured signal is silent (peak {20*np.log10(cap_peak) if cap_peak > 0 else float('-inf'):.1f} dBFS) -- "
              f"the recording likely failed (wrong input device, permissions, "
              f"routing not actually reaching the recorder, etc.). Aborting "
              f"before the null test, which cannot distinguish this from a "
              f"genuine pass. ***")
        result["verdict"] = "CAPTURED_SILENCE"
        return result
    if abs(corr_coeff) < CORRELATION_THRESHOLD:
        print(f"\n*** Captured signal does not correlate with the reference at any "
              f"alignment (correlation coefficient {corr_coeff:.4f} at the best-found "
              f"offset) -- this isn't a subtle filtering difference, the two files "
              f"don't appear to contain the same signal at all. Check routing/device "
              f"selection before trusting anything below. ***")
        result["verdict"] = "NO_CORRELATION"
        return result

    # --- 1. Direct sample-by-sample diff (no gain correction) ---------------
    print(f"\n--- Direct diff (aligned, no gain correction) ---")
    raw_diff, diff_report = direct_diff_report(ref_a, cap_a, sr, exact_tolerance_lsb24)
    print_direct_diff_report(diff_report)
    result["direct_diff"] = diff_report

    # --- 2. Gain-corrected null test -----------------------------------------
    print(f"\n--- Gain-corrected null test ---")
    denom = np.dot(ref_a, ref_a)
    gain = float(np.dot(ref_a, cap_a) / denom) if denom > 0 else 1.0
    print(f"Best-fit linear gain (captured = gain * reference): {gain:.9f} "
          f"({20 * np.log10(abs(gain)):+.6f} dB)")
    result["gain"] = gain

    residual = cap_a - gain * ref_a
    worst_residual_sample = int(np.argmax(np.abs(residual))) if len(residual) else None
    peak_db, rms_db = dbfs(residual)
    print(f"Null-test residual (whole aligned overlap): peak {peak_db:.1f} dBFS, "
          f"RMS {rms_db:.1f} dBFS")
    result["residual_peak_dbfs"] = peak_db
    result["residual_rms_dbfs"] = rms_db

    hf_frac_res = hf_energy_fraction(residual, sr, hf_hz)
    hf_frac_ref = hf_energy_fraction(ref_a, sr, hf_hz)
    print(f"Residual energy at/above {hf_hz:.0f} Hz: {hf_frac_res*100:.4f}% of "
          f"total residual energy")
    print(f"(reference signal itself has {hf_frac_ref*100:.4f}% of its own energy "
          f"at/above {hf_hz:.0f} Hz, for scale)")
    result["hf_energy_fraction_residual"] = hf_frac_res
    result["hf_energy_fraction_reference"] = hf_frac_ref

    if peak_db < -100:
        verdict = "PASS"
        print("\n=> PASS: capture is bit-perfect to the noise floor (no filtering detected).")
    elif hf_frac_res > 2 * hf_frac_ref:
        verdict = "FAIL_HF_FILTERING"
        print("\n=> Residual energy is disproportionately concentrated at/above "
              f"{hf_hz:.0f} Hz relative to the reference's own spectral balance "
              "-- consistent with an HF-selective filter somewhere in the path.")
    else:
        verdict = "FAIL_BROADBAND"
        print("\n=> Residual is broadband (not HF-concentrated) -- look for gain/alignment "
              "issues, dropouts, or noise rather than a frequency-selective filter.")
    result["verdict"] = verdict

    # Spectral comparison
    nperseg = min(65536, len(ref_a))
    f_ref, p_ref = welch(ref_a, fs=sr, nperseg=nperseg)
    _, p_cap = welch(cap_a, fs=sr, nperseg=nperseg)
    with np.errstate(divide="ignore"):
        db_ref = 10 * np.log10(p_ref + 1e-24)
        db_cap = 10 * np.log10(p_cap + 1e-24)
    diff_db = db_cap - db_ref

    band_mask = (f_ref >= hf_hz - 2000) & (f_ref <= sr / 2 * 0.99)
    if np.any(band_mask):
        idx = np.where(band_mask)[0]
        step = max(1, len(idx) // 20)
        print(f"\nCaptured-vs-reference level difference near/above {hf_hz:.0f} Hz:")
        for i in idx[::step]:
            print(f"  {f_ref[i]:8.0f} Hz: {diff_db[i]:+6.2f} dB")
        result["band_table_hz"] = [float(f_ref[i]) for i in idx[::step]]
        result["band_table_db"] = [float(diff_db[i]) for i in idx[::step]]

    if plot:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt

        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 8), sharex=True)
        ax1.semilogx(f_ref, db_ref, label="reference")
        ax1.semilogx(f_ref, db_cap, label="captured", alpha=0.8)
        ax1.axvline(hf_hz, color="red", linestyle="--", linewidth=0.8)
        ax1.set_ylabel("PSD (dB, arbitrary reference)")
        ax1.legend()
        ax1.set_title("Spectra: reference vs. captured")

        ax2.semilogx(f_ref, diff_db)
        ax2.axvline(hf_hz, color="red", linestyle="--", linewidth=0.8)
        ax2.set_xlabel("Frequency (Hz)")
        ax2.set_ylabel("captured - reference (dB)")
        ax2.set_title("Measured magnitude response of the loopback path")
        fig.tight_layout()
        fig.savefig(plot, dpi=150)
        print(f"\nSaved spectral plot to {plot}")
        result["plot"] = plot

    if waveform_plot:
        zoom_center = worst_residual_sample if worst_residual_sample is not None \
            else diff_report["peak_diff_sample"]
        save_waveform_plot(ref_a, cap_a, raw_diff, sr, waveform_plot, gain=gain,
                            residual=residual, zoom_center_sample=zoom_center)
        print(f"Saved waveform plot to {waveform_plot}")
        result["waveform_plot"] = waveform_plot

    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("reference")
    ap.add_argument("captured")
    ap.add_argument("--hf-hz", type=float, default=20000.0,
                     help="frequency threshold for the disputed 'high band' (default 20000)")
    ap.add_argument("--max-lag-seconds", type=float, default=2.0,
                     help="search window for the unknown start offset (default 2s)")
    ap.add_argument("--plot", default=None, help="optional path to save a PNG spectral plot")
    ap.add_argument("--waveform-plot", default=None,
                     help="optional path to save a PNG time-domain waveform plot "
                          "(aligned reference vs. captured, raw diff, and a zoomed-in "
                          "view around the worst-diff sample)")
    ap.add_argument("--exact-tolerance-lsb24", type=float, default=0.5,
                     help="direct-diff tolerance in fractions of a 24-bit LSB, to absorb "
                          "harmless int<->float conversion rounding (default 0.5; use 0 "
                          "to require literal floating-point equality)")
    args = ap.parse_args()

    run_comparison(args.reference, args.captured, hf_hz=args.hf_hz,
                    max_lag_seconds=args.max_lag_seconds, plot=args.plot,
                    waveform_plot=args.waveform_plot,
                    exact_tolerance_lsb24=args.exact_tolerance_lsb24)


if __name__ == "__main__":
    main()
