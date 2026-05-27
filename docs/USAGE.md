# timepitch-bench — usage

Every published result in the writeups can be reproduced by running one
command from a built tree. This document maps each result to the command
that produces it.

The benchmark binary builds to `./build/timepitch_bench` after running
`cmake -S . -B build && cmake --build build -j`.

All commands write a JSON + `.txt` mirror into `./reports/` by default.
Add `--no-save` to suppress that, or `--out-dir <path>` to redirect.

---

## 1. Stretcher tests (Signalsmith / SoundTouch / Rubber Band)

By default each invocation runs every enabled stretcher in turn and
prints a side-by-side table.

### 1.1 Baseline battery — seven signals, all libraries

```sh
# Sine 1.5x time-stretch
./build/timepitch_bench --signal sine --time-ratio 1.5

# Pitch +1 octave (sine reference)
./build/timepitch_bench --signal sine --pitch-scale 2.0

# Pitch −1 octave
./build/timepitch_bench --signal sine --pitch-scale 0.5

# Sweep 1.5x time-stretch
./build/timepitch_bench --signal sweep --time-ratio 1.5

# White noise, pitch shift, larger blocks
./build/timepitch_bench --signal noise --pitch-scale 2.0 --block-size 1024

# Impulse 1.5x
./build/timepitch_bench --signal impulse --time-ratio 1.5

# Identity (neutral 1.0x)
./build/timepitch_bench --signal sine --time-ratio 1.0 --pitch-scale 1.0
```

These produce the realtime-factor / pitch-accuracy / peak-RSS / latency
columns reported under "Baseline battery."

### 1.2 Algorithmic latency (each library's self-report)

```sh
./build/timepitch_bench --signal sine --measure-latency
```

Latency is in frames; divide by `--sample-rate` (default 48000) for
seconds. Reproduces the latency footnotes in the writeup.

### 1.3 Stationary Shepard — the envelope-preservation metric

Stationary mode disables the continuous octave sweep, so the analysis
window catches stationary peaks. This is the clean test.

```sh
# Identity (sanity check — all libs should be sub-cent / sub-0.01 oct)
./build/timepitch_bench --signal shepard --shepard-sweep-rate 0 \
    --time-ratio 1.0 --pitch-scale 1.0

# Pitch ×1.3348 (probes envelope shift without partial overlap)
./build/timepitch_bench --signal shepard --shepard-sweep-rate 0 \
    --pitch-scale 1.3348

# Pitch ×2.0 (one octave — Shepard-illusion case)
./build/timepitch_bench --signal shepard --shepard-sweep-rate 0 \
    --pitch-scale 2.0

# Time-stretch ×1.5 (should leave envelope unchanged)
./build/timepitch_bench --signal shepard --shepard-sweep-rate 0 \
    --time-ratio 1.5
```

Reads off the report: `quality.shepard.center_error_cents` and
`quality.shepard.sigma_error_oct` per library. These populate the
"Stationary Shepard" tables in the writeup.

### 1.4 Sweeping Shepard — window-vs-latency stress

```sh
# Same four configurations as stationary, but with the 0.5 oct/sec sweep
./build/timepitch_bench --signal shepard \
    --time-ratio 1.0 --pitch-scale 1.0          # identity
./build/timepitch_bench --signal shepard --pitch-scale 1.3348
./build/timepitch_bench --signal shepard --pitch-scale 2.0
./build/timepitch_bench --signal shepard --time-ratio 1.5
```

`--shepard-sweep-rate` defaults to 0.5 (oct/sec) — omit to use the
default sweep. Reproduces the "Sweeping Shepard" tables, including the
predicted +40-cent identity error on Signalsmith (its 60 ms latency ×
0.5 oct/sec).

### 1.5 Single-library run (faster iteration)

```sh
./build/timepitch_bench --library rubberband --signal shepard \
    --shepard-sweep-rate 0 --pitch-scale 1.3348
```

`--list` enumerates available stretchers under the current build
configuration.

---

## 2. Resampler tests (libsamplerate / r8brain)

The resampler harness is a separate mode entered with `--resample`.
Every invocation runs both libraries in turn.

```sh
./build/timepitch_bench --resample --list-resamplers
```

### 2.1 In-band sine SNR

```sh
# 22050 → 48000
./build/timepitch_bench --resample --resample-signal sine \
    --src-rate 22050 --dst-rate 48000 --sine-hz 1000

# 44100 → 48000
./build/timepitch_bench --resample --resample-signal sine \
    --src-rate 44100 --dst-rate 48000 --sine-hz 1000

# 88200 → 48000
./build/timepitch_bench --resample --resample-signal sine \
    --src-rate 88200 --dst-rate 48000 --sine-hz 1000

# 96000 → 48000 (2:1 — float-precision floor)
./build/timepitch_bench --resample --resample-signal sine \
    --src-rate 96000 --dst-rate 48000 --sine-hz 1000
```

Reads off `quality.resample.snr_db` per library. Reproduces the "In-band
1 kHz sine — spectral SNR" table.

### 2.2 Alias rejection (downsample only)

The runner places a sine slightly above the destination Nyquist and below
the source Nyquist. Any output is leakage.

```sh
./build/timepitch_bench --resample --resample-signal alias \
    --src-rate 88200 --dst-rate 48000
./build/timepitch_bench --resample --resample-signal alias \
    --src-rate 96000 --dst-rate 48000
```

Reads `quality.resample.alias_rms_dbfs`.

### 2.3 Impulse-smear length

```sh
./build/timepitch_bench --resample --resample-signal impulse \
    --src-rate 22050 --dst-rate 48000
./build/timepitch_bench --resample --resample-signal impulse \
    --src-rate 44100 --dst-rate 48000
./build/timepitch_bench --resample --resample-signal impulse \
    --src-rate 88200 --dst-rate 48000
./build/timepitch_bench --resample --resample-signal impulse \
    --src-rate 96000 --dst-rate 48000
```

Reads `quality.resample.impulse_pre_samples` and `impulse_post_samples`
(samples between peak and the −60 dB envelope crossing on each side).

### 2.4 Throughput

Throughput is reported in `timing.wall_seconds` for every resample run
above. The "Throughput" table in the writeup is just the same SNR
configurations from §2.1 with wall time read instead of SNR.

---

## 3. Charts

Once you have a populated `reports/` directory:

```sh
pip install --user matplotlib
python3 docs/make_charts.py
python3 docs/make_charts.py --reports /path/to/reports --out /tmp/charts
```

The script picks the most recent matching report per
(signal, sweep_rate, time_ratio, pitch_scale) tuple, so new runs
automatically supersede older ones in the figures.

---

## 4. Sweeping a parameter (suggested workflow)

There is no built-in sweep mode. Drive sweeps from the shell:

```sh
for ratio in 0.5 0.75 1.0 1.25 1.5 1.75 2.0; do
  ./build/timepitch_bench --signal shepard --shepard-sweep-rate 0 \
      --pitch-scale "$ratio"
done
python3 docs/make_charts.py
```

Filenames are timestamped + parameter-tagged, so collisions don't happen
and `make_charts.py` will pick up the new data automatically.
