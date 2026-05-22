# dsp-bench

Performance and quality benchmark harness comparing time-stretch / pitch-shift
libraries under consideration for MondoLoop:

- **Signalsmith Stretch** (MIT)
- **SoundTouch** (LGPL)
- **Rubber Band** (LGPL / commercial)

## Build

```sh
cmake -S . -B build
cmake --build build -j
```

The build pulls all three libraries via CMake `FetchContent`. To disable any
of them (e.g. if network access is restricted or a library fails to build):

```sh
cmake -S . -B build -DBENCH_ENABLE_RUBBERBAND=OFF
```

## Run

```sh
# Compare all three at 1.5x slower, 1 kHz sine
./build/dsp_bench --signal sine --time-ratio 1.5

# Pitch-shift up an octave on noise, larger blocks
./build/dsp_bench --signal noise --pitch-scale 2.0 --block-size 1024

# Just one library, report latency
./build/dsp_bench --library signalsmith --measure-latency
```

## Metrics

- **CPU**: wall-clock total, mean per call, p95 per call, realtime factor.
- **Memory**: peak RSS (Linux `getrusage`).
- **Quality**: for sine input, detected output fundamental and pitch error
  in cents vs the expected value (1000 Hz * pitch_scale).
- **Latency** (opt-in via `--measure-latency`): each library's self-reported
  algorithmic latency in frames.

## Reports & provenance

For the rationale behind the flow below — why JSON per run, why always-save,
why a separate commit script, etc. — see
[docs/reports_design.md](docs/reports_design.md).

Each run writes a JSON report (plus a `.txt` mirror of the stdout) into
`./reports/` by default. Filenames embed timestamp and parameters so
results can be correlated across runs, machines, library versions, and
upstream code changes:

```
2026-05-21T19-06-30Z_shepard_t1.000_p1.000_b512_sr48000_sweep0.500.json
```

The JSON captures:

- `params` — full RunOptions (signal, ratios, block size, sample rate,
  sweep rate, etc.)
- `build` — compiler ID/version/flags, CMake build type, build timestamp,
  dsp-bench git short SHA, and dirty flag
- `libraries` — FetchContent-resolved git SHA and tag for each of
  signalsmith / soundtouch / rubberband
- `host` — hostname, uname, CPU model and feature flags, logical-core count
- `results` — per-library timing, peak RSS, optional reported latency, and
  the signal-specific quality block (sine, shepard, …)

Opt out per-run with `--no-save`. Override the directory with
`--out-dir <path>`.

### Committing reports

`scripts/commit-reports.sh` finds untracked report files under
`dsp-bench/reports/` and commits them with an auto-generated description
pulled from each JSON's metadata (host, library revs, params):

```sh
# all-in-one commit with a free-text note
./scripts/commit-reports.sh "rubber band fft-tweak A/B"

# one commit per report
./scripts/commit-reports.sh --individual

# preview without committing
./scripts/commit-reports.sh --dry-run
```

This is the workflow for collecting evidence to attach to an upstream PR
(e.g. demonstrating that a code change in Rubber Band moves a measurable
metric on a known signal).

## Charts

`docs/make_charts.py` is a docs-only tool that ingests `reports/*.json`
and renders the charts referenced in `docs/shepard_findings.md`. It picks
the most recent matching report per (signal, sweep_rate, time_ratio,
pitch_scale) tuple, so adding new reports automatically supersedes old
ones in the figures.

```sh
python3 docs/make_charts.py
python3 docs/make_charts.py --reports /path/to/reports --out /tmp/charts
python3 docs/make_charts.py reports/some_specific_run.json ...
```

Requires `matplotlib` (`pip install --user matplotlib`). Not a runtime
dependency of the benchmark itself.

## Next steps

1. Run on flagship Android hardware (Pixel 8/9) via an NDK port — the
   measurement core is platform-agnostic; only `peak_rss_kb()` needs an
   Android variant.
2. Add transient-preservation scoring if drum loops get added later
   (currently the corpus is synthetic only per the design decision).
