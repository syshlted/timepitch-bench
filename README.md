# timepitch-bench

A reproducible performance and quality benchmark harness for time-stretch /
pitch-shift and sample-rate-conversion libraries. Originally built to inform
DSP-library selection in the [MondoLoop](https://systemhalted.com/blog) mobile
DAW project; extracted here so the methodology and numbers can be reproduced
independently.

**Time-stretch / pitch-shift libraries compared:**

- [Signalsmith Stretch](https://github.com/Signalsmith-Audio/signalsmith-stretch) — MIT
- [SoundTouch](https://www.surina.net/soundtouch/) — LGPL
- [Rubber Band](https://breakfastquay.com/rubberband/) — GPL / commercial

**Sample-rate converters compared:**

- [libsamplerate](https://github.com/libsndfile/libsamplerate) — BSD-2-Clause
- [r8brain-free-src](https://github.com/avaneev/r8brain-free-src) — MIT

### License

This benchmark harness is licensed under **Apache-2.0** (see
[LICENSE](LICENSE)). It does *not* relicense the libraries it benchmarks:
each is acquired at build time from its upstream and remains under its
own license. In particular, **enabling Rubber Band binds the resulting
binary to GPL or to Rubber Band's commercial license** — if that's a
problem for your use, build with `-DBENCH_ENABLE_RUBBERBAND=OFF`.

## Dependencies

No upstream library source is committed to this repository. Each library
is acquired at configure time according to the following priority order:

1. **Local checkout** under `./external/<lib>/` (if present)
2. **System package** via `pkg-config` (only if `-DBENCH_USE_SYSTEM_LIBS=ON`)
3. **FetchContent download** from upstream Git (the default fallback)

Each dependency emits one status line at configure time identifying which
source it resolved to, e.g.:

```
-- [deps] signalsmith-stretch: FetchContent (github.com/Signalsmith-Audio/signalsmith-stretch.git @ main)
-- [deps] libsamplerate:       system package (pkg-config samplerate 0.2.2)
-- [deps] r8brain:             local checkout (/home/me/timepitch-bench/external/r8brain)
```

### The libraries

| Library                                                                                   | Pinned ref     | License        | System pkg-config name | Notes |
|-------------------------------------------------------------------------------------------|---------------|----------------|------------------------|-------|
| [Signalsmith Stretch](https://github.com/Signalsmith-Audio/signalsmith-stretch)           | `main`        | MIT            | *(not packaged)*       | Header-mostly. Must be fetched or pre-staged. |
| [SoundTouch](https://codeberg.org/soundtouch/soundtouch)                                  | `2.3.3`       | LGPL           | `soundtouch`           | Packaged on most Linux distros (`libsoundtouch-dev` / `soundtouch-devel`). |
| [Rubber Band](https://github.com/breakfastquay/rubberband)                                | `v4.0.0`      | GPL / commercial | `rubberband`         | Packaged on most Linux distros (`librubberband-dev`). |
| [libsamplerate](https://github.com/libsndfile/libsamplerate)                              | `0.2.2`       | BSD-2-Clause   | `samplerate`           | Packaged on most Linux distros (`libsamplerate0-dev` / `libsamplerate-devel`). |
| [r8brain-free-src](https://github.com/avaneev/r8brain-free-src)                           | `version-6.5` | MIT            | *(not packaged)*       | Must be fetched or pre-staged. |

### Offline / locked-down networks

Pre-stage everything from a machine with internet access:

```sh
scripts/fetch-deps.sh
```

This clones each pinned upstream into `./external/<lib>/`. CMake detects
this directory and skips downloading. Re-running the script is safe — it
skips any directory that already exists.

You can also point the script at a custom location:

```sh
scripts/fetch-deps.sh /shared/timepitch-deps
ln -s /shared/timepitch-deps ./external
```

### System packages

Prefer system-installed libraries (where available) by passing
`-DBENCH_USE_SYSTEM_LIBS=ON` at configure time:

```sh
# Debian / Ubuntu
sudo apt install libsoundtouch-dev librubberband-dev libsamplerate0-dev pkg-config
cmake -S . -B build -DBENCH_USE_SYSTEM_LIBS=ON
cmake --build build -j
```

```sh
# Fedora
sudo dnf install soundtouch-devel rubberband-devel libsamplerate-devel pkgconfig
cmake -S . -B build -DBENCH_USE_SYSTEM_LIBS=ON
cmake --build build -j
```

```sh
# macOS (Homebrew)
brew install soundtouch rubberband libsamplerate pkg-config
cmake -S . -B build -DBENCH_USE_SYSTEM_LIBS=ON
cmake --build build -j
```

Signalsmith Stretch and r8brain-free are not packaged anywhere we know
of; those two always come from either `./external/` or FetchContent,
regardless of `BENCH_USE_SYSTEM_LIBS`.

If `BENCH_USE_SYSTEM_LIBS=ON` is set but none of the three packageable
libraries are found, configure emits a warning and falls back to
FetchContent. Install the development packages above to silence it.

### Disabling individual wrappers

Each library can be disabled independently — useful for builds where
network access is restricted or a license is incompatible with your
distribution:

```sh
cmake -S . -B build \
    -DBENCH_ENABLE_RUBBERBAND=OFF \
    -DBENCH_ENABLE_SOUNDTOUCH=OFF
```

The remaining wrappers still build; the disabled ones simply don't get
acquired or linked.

## Build

```sh
cmake -S . -B build
cmake --build build -j
```

The default configure resolves every dependency for you. To control where
they come from (system packages, pre-staged checkout, FetchContent), or to
disable individual wrappers, see the [Dependencies](#dependencies) section
above.

## Run

```sh
# Compare all three at 1.5x slower, 1 kHz sine
./build/timepitch_bench --signal sine --time-ratio 1.5

# Pitch-shift up an octave on noise, larger blocks
./build/timepitch_bench --signal noise --pitch-scale 2.0 --block-size 1024

# Just one library, report latency
./build/timepitch_bench --library signalsmith --measure-latency
```

Every test from the published writeups can be reproduced one-to-one.
See [docs/USAGE.md](docs/USAGE.md) for the per-test command recipes.

Independent test results are welcome — see [CONTRIBUTING.md](CONTRIBUTING.md)
for how to submit them.

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
  timepitch-bench git short SHA, and dirty flag
- `libraries` — FetchContent-resolved git SHA and tag for each of
  signalsmith / soundtouch / rubberband
- `host` — hostname, uname, CPU model and feature flags, logical-core count
- `results` — per-library timing, peak RSS, optional reported latency, and
  the signal-specific quality block (sine, shepard, …)

Opt out per-run with `--no-save`. Override the directory with
`--out-dir <path>`.

### Committing reports

`scripts/commit-reports.sh` finds untracked report files under
`timepitch-bench/reports/` and commits them with an auto-generated description
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
