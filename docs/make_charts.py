#!/usr/bin/env python3
"""Render charts from timepitch-bench JSON reports.

By default, scans ../reports/ for *.json, picks one stationary-Shepard run
per (pitch_scale, time_ratio) combination, and emits the same set of PNGs
used in shepard_findings.md. If multiple reports match a combination, the
most recent (by timestamp_utc) is used.

Override the source dir with --reports, or pass an explicit list of JSON
paths positionally to chart exactly those.
"""

import argparse
import glob
import json
import math
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

LIBS = ["signalsmith", "soundtouch", "rubberband"]
COLORS = {"signalsmith": "#4C72B0", "soundtouch": "#DD8452", "rubberband": "#55A868"}

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_REPORTS = os.path.join(HERE, "..", "reports")
DEFAULT_OUT     = os.path.join(HERE, "charts")

# Tests we expect to chart, keyed by (signal, sweep_rate, time_ratio, pitch_scale).
STATIONARY_TESTS = [
    ("identity",     ("shepard", 0.0, 1.0, 1.0)),
    ("pitch 1.3348", ("shepard", 0.0, 1.0, 1.3348)),
    ("pitch 2.0",    ("shepard", 0.0, 1.0, 2.0)),
    ("time 1.5x",    ("shepard", 0.0, 1.5, 1.0)),
]
SWEEPING_TESTS = [
    ("identity",     ("shepard", 0.5, 1.0, 1.0)),
    ("pitch 1.3348", ("shepard", 0.5, 1.0, 1.3348)),
    ("pitch 2.0",    ("shepard", 0.5, 1.0, 2.0)),
    ("time 1.5x",    ("shepard", 0.5, 1.5, 1.0)),
]

def load_reports(report_paths):
    reports = []
    for p in report_paths:
        with open(p) as f:
            reports.append(json.load(f))
    return reports

def best_match(reports, key):
    signal, sweep, tr, ps = key
    cands = []
    for r in reports:
        p = r["params"]
        if (p["signal"] == signal and
            math.isclose(p["shepard_sweep_rate"], sweep, abs_tol=1e-6) and
            math.isclose(p["time_ratio"], tr, abs_tol=1e-6) and
            math.isclose(p["pitch_scale"], ps, abs_tol=1e-4)):
            cands.append(r)
    if not cands:
        return None
    return max(cands, key=lambda r: r["timestamp_utc"])

def shepard_metric(result, key):
    """Pull out (center_err_cents, sigma_err_oct) from a result's shepard fit."""
    q = result["quality"].get("shepard")
    if not q or not q["envelope"]["fit_ok"] or not q["envelope"]["input_fit_ok"]:
        return None, None
    out_mu, in_mu = q["envelope"]["center_hz"], q["envelope"]["input_center_hz"]
    out_s, in_s   = q["envelope"]["sigma_oct"], q["envelope"]["input_sigma_oct"]
    pitch = key[3]
    observed_shift = math.log2(out_mu / in_mu)
    expected_shift = math.log2(pitch)
    return (observed_shift - expected_shift) * 1200.0, out_s - in_s

def collect(reports, test_list):
    """Returns (tests, center_err, sigma_err, missing) keyed per library."""
    tests, ce, se, missing = [], {l: [] for l in LIBS}, {l: [] for l in LIBS}, []
    for label, key in test_list:
        rep = best_match(reports, key)
        if rep is None:
            missing.append(label)
            continue
        tests.append(label)
        by_lib = {r["library"]: r for r in rep["results"]}
        for lib in LIBS:
            if lib in by_lib:
                c, s = shepard_metric(by_lib[lib], key)
                ce[lib].append(c if c is not None else float("nan"))
                se[lib].append(s if s is not None else float("nan"))
            else:
                ce[lib].append(float("nan"))
                se[lib].append(float("nan"))
    return tests, ce, se, missing

def grouped_bar(tests, data_by_lib, title, ylabel, fname, out_dir,
                hline=None, yfmt="{:+.1f}"):
    fig, ax = plt.subplots(figsize=(8.5, 4.5))
    x = np.arange(len(tests))
    width = 0.26
    for i, lib in enumerate(LIBS):
        offset = (i - 1) * width
        vals = data_by_lib[lib]
        bars = ax.bar(x + offset, vals, width, label=lib, color=COLORS[lib])
        for b, v in zip(bars, vals):
            if math.isnan(v): continue
            ax.text(b.get_x() + b.get_width() / 2, v,
                    yfmt.format(v),
                    ha="center",
                    va="bottom" if v >= 0 else "top",
                    fontsize=8)
    if hline is not None:
        ax.axhline(hline, color="#888", linewidth=0.8, linestyle="--")
    ax.set_xticks(x)
    ax.set_xticklabels(tests)
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.legend(loc="best", fontsize=9)
    ax.grid(axis="y", alpha=0.3)
    fig.tight_layout()
    fig.savefig(os.path.join(out_dir, fname), dpi=130)
    plt.close(fig)

def cpu_vs_quality(reports, out_dir):
    """Scatter: realtime factor under pitch shift vs |avg center error|."""
    pitch_keys = [STATIONARY_TESTS[1][1], STATIONARY_TESTS[2][1]]
    rtfs = {l: [] for l in LIBS}
    cerrs = {l: [] for l in LIBS}
    for key in pitch_keys:
        rep = best_match(reports, key)
        if rep is None: continue
        for r in rep["results"]:
            lib = r["library"]
            if lib not in LIBS: continue
            rtfs[lib].append(r["timing"]["realtime_factor"])
            c, _ = shepard_metric(r, key)
            if c is not None:
                cerrs[lib].append(abs(c))

    fig, ax = plt.subplots(figsize=(7.5, 5.0))
    for lib in LIBS:
        if not rtfs[lib] or not cerrs[lib]:
            continue
        x = sum(rtfs[lib]) / len(rtfs[lib])
        y = sum(cerrs[lib]) / len(cerrs[lib])
        ax.scatter(x, y, s=200, color=COLORS[lib], label=lib,
                   edgecolor="black", linewidth=0.6)
        ax.annotate(lib, xy=(x, y), xytext=(8, 6),
                    textcoords="offset points", fontsize=10)
    ax.set_xscale("log")
    ax.set_xlabel("realtime factor under pitch shift (log scale)")
    ax.set_ylabel("|center error| averaged across pitch tests (cents)")
    ax.set_title("CPU vs quality trade-off — Shepard pitch tests")
    ax.grid(True, which="both", alpha=0.3)
    ax.set_ylim(bottom=0)
    fig.tight_layout()
    fig.savefig(os.path.join(out_dir, "cpu_vs_quality.png"), dpi=130)
    plt.close(fig)

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--reports", default=DEFAULT_REPORTS,
                    help="directory containing benchmark JSON reports")
    ap.add_argument("--out", default=DEFAULT_OUT,
                    help="output directory for chart PNGs")
    ap.add_argument("paths", nargs="*",
                    help="explicit list of JSON files (overrides --reports)")
    args = ap.parse_args()

    if args.paths:
        paths = args.paths
    else:
        paths = sorted(glob.glob(os.path.join(args.reports, "*.json")))

    if not paths:
        print(f"no reports found in {args.reports}", file=sys.stderr)
        sys.exit(1)

    reports = load_reports(paths)
    os.makedirs(args.out, exist_ok=True)

    tests, ce, se, missing = collect(reports, STATIONARY_TESTS)
    if tests:
        grouped_bar(tests, ce,
                    "Stationary Shepard — envelope center error vs input",
                    "center error (cents)", "center_error.png", args.out, hline=0)
        grouped_bar(tests, se,
                    "Stationary Shepard — envelope sigma error vs input",
                    "sigma error (octaves)", "sigma_error.png", args.out,
                    hline=0, yfmt="{:+.3f}")
    if missing:
        print(f"warning: missing stationary runs: {missing}", file=sys.stderr)

    tests_s, ce_s, se_s, missing_s = collect(reports, SWEEPING_TESTS)
    if tests_s:
        grouped_bar(tests_s, ce_s,
                    "Sweeping Shepard — envelope center error vs input (noisier)",
                    "center error (cents)", "center_error_sweep.png", args.out, hline=0)
    if missing_s:
        print(f"warning: missing sweeping runs: {missing_s}", file=sys.stderr)

    cpu_vs_quality(reports, args.out)
    print(f"wrote charts to {args.out}")

if __name__ == "__main__":
    main()
