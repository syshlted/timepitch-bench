# Contributing to timepitch-bench

Thank you for considering a contribution. The most valuable thing
outside contributors can give this project is **independent test
results** — running the same benchmark configurations on different
hardware, OS, compiler, and library revisions, then submitting the
generated reports.

That comparison data is what makes the published numbers verifiable
rather than merely declared.

---

## Submitting test results

### TL;DR

1. Build the bench (see [docs/USAGE.md](docs/USAGE.md)).
2. Run the configurations you care about — or the full reproduction
   suite (see [docs/USAGE.md](docs/USAGE.md) §1 and §2).
3. Attach the resulting `reports/*.json` files to a GitHub issue titled
   `Results: <hardware/OS short description>`.

That's the minimum. Everything below explains what the JSON already
captures for you, and what to add by hand if it's relevant.

### What the report JSON captures automatically

Every run writes a self-describing report. You do **not** need to type
any of this out — it is captured at runtime:

| Section      | Captured fields |
|--------------|-----------------|
| `params`     | signal, time_ratio, pitch_scale, block_size, sample_rate, shepard_sweep_rate, src_rate, dst_rate, duration, etc. — every CLI option used |
| `build`      | compiler ID + version (e.g. `GNU 15.0.1`), compile flags, build type (Debug/Release/etc.), build timestamp, `timepitch_bench` git short SHA, dirty flag |
| `libraries`  | FetchContent-resolved git SHA and tag of every library linked in (signalsmith / soundtouch / rubberband / libsamplerate / r8brain) |
| `host`       | hostname, `uname` output (OS name, kernel version, machine arch), CPU model, CPU feature flags (SSE/AVX/NEON/etc.), logical-core count |
| `results`    | per-library timing (wall, mean per call, p95), peak RSS, optional reported algorithmic latency, and the signal-specific quality block (sine pitch error, Shepard envelope metrics, resampler SNR/alias/impulse) |

This is intentional: a submitted report is a complete, machine-readable
fingerprint of the run. You should not need to reformat or annotate it.

### What to add by hand in the submission

Most of these are environmental factors the benchmark can't observe
directly. Include them in the GitHub issue body, not in the JSON:

- **Thermal / power state**: laptop on AC or battery? Was the machine
  warm from prior workload? CPU governor (e.g. Linux `performance`,
  `powersave`, `schedutil`)?
- **System load during run**: was the machine otherwise idle, or was
  there background activity (build jobs, browser, VM)?
- **Memory total**: helpful when reports show high peak RSS; the host
  block currently reports core count but not total RAM.
- **OS distribution + version**: `uname` captures kernel; please add
  the distribution string (e.g. `Fedora 42`, `Ubuntu 24.04 LTS`,
  `macOS 15.4`, `Windows 11 24H2`).
- **CPU microarchitecture name** (e.g. "Zen 4", "Apple M3 Pro",
  "Cortex-A720") — sometimes more diagnostic than the CPU model
  string the report captures.
- **Anything unusual**: cross-compile target, sanitiser enabled,
  unusual `-march=` flag, custom toolchain. The build block records
  the flags but human context helps reviewers.

### What you do *not* need to provide

- Hand-typed compiler version strings → captured.
- Hand-typed library versions → captured (as git SHAs).
- Hand-typed CPU model → captured.
- Reformatted result tables → the JSON is the canonical form;
  please don't paraphrase numbers.

### Suggested issue template

```
**Hardware**: <short description, e.g. "MacBook Pro M3, 18 GB RAM">
**OS / distro**: <e.g. "macOS 15.4">
**Power state during run**: <AC / battery, governor, idle/loaded>
**Anything unusual**: <none / cross-compile / sanitiser / custom flags>

Attached: reports/<run files>.json
```

Attach the JSON files directly. The `.txt` mirrors are optional;
the JSON is canonical.

### Bulk submissions

If you ran many configurations (e.g. the full reproduction suite),
either:

- attach the whole `reports/` directory as a tarball, or
- open a PR adding the JSON files to `reports/contributed/<your-handle>/`.

Bulk PRs are welcome. Keep the directory shallow and the filenames
unchanged — the timestamp + parameter tags are how `make_charts.py`
correlates them.

---

## Code contributions

### Bug reports

Open an issue with:

- the command you ran,
- the `report.json` if one was generated (or the stdout if not),
- what you expected vs what happened.

### Adding a new library to compare

The bench is structured so that adding a new stretcher or resampler is
isolated to one new wrapper file. See:

- [src/stretcher.h](src/stretcher.h) — the interface every stretcher
  implements. Existing wrappers
  ([stretcher_signalsmith.cpp](src/stretcher_signalsmith.cpp),
  [stretcher_soundtouch.cpp](src/stretcher_soundtouch.cpp),
  [stretcher_rubberband.cpp](src/stretcher_rubberband.cpp)) are short
  enough to use as templates.
- [src/resampler.h](src/resampler.h) — same shape for resamplers.
- [CMakeLists.txt](CMakeLists.txt) — `FetchContent_Declare` plus a
  `BENCH_ENABLE_<NAME>` option, mirroring the existing entries.
- [src/resampler_registry.cpp](src/resampler_registry.cpp) /
  the equivalent stretcher registry — register the new wrapper.

Please include:

1. A short justification in the PR description: who maintains the
   library, license, why it's worth adding.
2. At least one report run on your machine showing it produces sane
   output (identity should be near-perfect on every metric).

### Modifying methodology

Changes to the signal generators, peak picker, Gaussian fit, or
quality scoring are welcome but need to be invariant-preserving.
Specifically, identity passes (no transform) should still return
**sub-cent center error and sub-0.01-octave sigma error on all three
stretchers**, and resampler identity should still hit float-precision
SNR floor. Include a before/after run of the identity test in the PR.

---

## Code style

- C++17, four-space indent, no tabs.
- Header order: corresponding header first, then standard, then
  third-party, then project. (Loose; not enforced by linter.)
- Comments only where the *why* isn't obvious from the code. Avoid
  restating what the next line does.
- Match the surrounding code's tone; nothing here is exotic.

---

## License of contributions

By submitting a contribution (issue, PR, test results, or report file)
you agree it may be distributed under this project's
[Apache-2.0 license](LICENSE).

Report files contributed under `reports/contributed/` are treated as
data and are also covered by Apache-2.0; please do not submit reports
that contain proprietary identifiers (custom hostnames, internal
machine names) you would not be comfortable publishing.
