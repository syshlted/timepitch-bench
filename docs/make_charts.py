#!/usr/bin/env python3
"""Render charts for shepard_findings.md from the 2026-05-20 measurements."""

import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "charts")
os.makedirs(OUT, exist_ok=True)

LIBS = ["signalsmith", "soundtouch", "rubberband"]
COLORS = {
    "signalsmith": "#4C72B0",
    "soundtouch":  "#DD8452",
    "rubberband":  "#55A868",
}

def grouped_bar(tests, data_by_lib, title, ylabel, fname,
                hline=None, yfmt="{:+.1f}"):
    fig, ax = plt.subplots(figsize=(8.5, 4.5))
    x = np.arange(len(tests))
    width = 0.26
    for i, lib in enumerate(LIBS):
        offset = (i - 1) * width
        vals = data_by_lib[lib]
        bars = ax.bar(x + offset, vals, width, label=lib, color=COLORS[lib])
        for b, v in zip(bars, vals):
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
    fig.savefig(os.path.join(OUT, fname), dpi=130)
    plt.close(fig)

# Stationary Shepard, center error in cents
tests = ["identity", "pitch 1.3348", "pitch 2.0", "time 1.5x"]
center_err = {
    "signalsmith": [-1.4,  75.6, -10.2,  19.0],
    "soundtouch":  [ 0.7, 172.9, 175.2, 106.1],
    "rubberband":  [-0.2,  42.9,  -3.2, -18.0],
}
grouped_bar(tests, center_err,
            "Stationary Shepard — envelope center error vs input",
            "center error (cents)", "center_error.png", hline=0)

# Stationary Shepard, sigma error in octaves
sigma_err = {
    "signalsmith": [ 0.001,  0.046,  0.092, -0.012],
    "soundtouch":  [-0.000, -0.071, -0.064, -0.062],
    "rubberband":  [-0.005,  0.030,  0.054,  0.011],
}
grouped_bar(tests, sigma_err,
            "Stationary Shepard — envelope sigma error vs input",
            "sigma error (octaves)", "sigma_error.png",
            hline=0, yfmt="{:+.3f}")

# Sweeping Shepard, center error in cents
center_err_sweep = {
    "signalsmith": [  40.3,  -7.0, -287.6, 145.3],
    "soundtouch":  [ -22.4,  95.0, -450.8, 234.3],
    "rubberband":  [ -20.2, -58.1, -318.4, 119.0],
}
grouped_bar(tests, center_err_sweep,
            "Sweeping Shepard — envelope center error vs input (noisier)",
            "center error (cents)", "center_error_sweep.png", hline=0)

# CPU vs quality scatter. Average |center error| across the two pitch tests
# (where stretchers actually do work), realtime factor averaged across the
# baseline battery's pitch-shift columns.
quality = {
    "signalsmith": (75.6 + 10.2) / 2,
    "soundtouch":  (172.9 + 175.2) / 2,
    "rubberband":  (42.9 + 3.2) / 2,
}
cpu_rtf = {  # realtime factor under pitch shift (mean of +1oct and -1oct)
    "signalsmith": (59 + 63) / 2,
    "soundtouch":  (247 + 245) / 2,
    "rubberband":  (9 + 10) / 2,
}

fig, ax = plt.subplots(figsize=(7.5, 5.0))
for lib in LIBS:
    ax.scatter(cpu_rtf[lib], quality[lib], s=200,
               color=COLORS[lib], label=lib, edgecolor="black", linewidth=0.6)
    ax.annotate(lib,
                xy=(cpu_rtf[lib], quality[lib]),
                xytext=(8, 6), textcoords="offset points",
                fontsize=10)
ax.set_xscale("log")
ax.set_xlabel("realtime factor under pitch shift (log scale, higher = faster CPU)")
ax.set_ylabel("|center error| averaged across pitch tests (cents)")
ax.set_title("CPU vs quality trade-off — Shepard pitch tests")
ax.grid(True, which="both", alpha=0.3)
ax.set_ylim(bottom=0)
fig.tight_layout()
fig.savefig(os.path.join(OUT, "cpu_vs_quality.png"), dpi=130)
plt.close(fig)

print("wrote charts to", OUT)
