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

## Next steps

1. Run on flagship Android hardware (Pixel 8/9) via an NDK port — the
   measurement core is platform-agnostic; only `peak_rss_kb()` needs an
   Android variant.
2. Add transient-preservation scoring if drum loops get added later
   (currently the corpus is synthetic only per the design decision).
