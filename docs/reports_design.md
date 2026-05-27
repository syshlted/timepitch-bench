# Reports & provenance — design rationale

This document records *why* the report-writing flow is shaped the way it
is, so future maintainers don't reinvent the constraints. Usage docs live
in [../README.md](../README.md); methodology / findings live in
[session_2026-05-20.md](session_2026-05-20.md) and
[shepard_findings.md](shepard_findings.md).

## What problem the flow solves

The benchmark is meant to do three things that share a single core need:
**every reported number must be reproducible and attributable.**

1. **Compare libraries** on the same hardware and parameters
   (the existing baseline use case).
2. **Compare the same library across machines** — desktop x86 today,
   flagship-tier Android NDK soon. The
   [latency stance](../../.. /MEMORY.md) targets parity on flagship
   Android only, so we need per-CPU evidence, not just average behavior.
3. **Prove upstream improvements.** If we patch Rubber Band (or another
   candidate) and submit a PR, we need to show a measurable, replicable
   metric delta on a defined signal. That means each run has to carry
   enough metadata for a third party to recreate it.

A flow that only prints to stdout would force every comparison to be
manual, ephemeral, and unauthenticated. The fix is structured
per-run reports with full provenance.

## Why per-run JSON + text mirror

Considered: JSONL (append-only), CSV, embedded SQLite.

- **JSONL** is great for streaming many runs into one file, but it makes
  *sharing one run* awkward — and the PR-attachment use case is exactly
  that. You'd either send the whole stream or carve out a line by hand.
- **CSV** can't represent nested data without flattening every quality
  metric, peak list, and CPU-flag list into columns. The schema becomes
  brittle as we add metrics.
- **SQLite** would be lovely for ad-hoc queries but is more machinery
  than this project warrants; it also can't be diff'd or skimmed.
- **JSON per run + a `.txt` mirror of stdout** keeps each run as a
  standalone artifact, human-readable on inspection, and trivial to ship
  with a PR. The text mirror is also the canonical interactive view —
  one source of truth for the printed output (`format_result()` in
  `runner.cpp`), used both for stdout and the file. No risk of the two
  drifting.

JSON written by hand (no library dependency). The schema is small enough
that a `<json/json.h>`-style dependency would have been overkill.

## Why "always save" with `--no-save` opt-out

Considered: opt-in `--out-dir` flag, no save by default.

The user explicitly chose always-save during scope-out. The reasoning
mattered: when collecting evidence — especially upstream-PR evidence —
the failure mode of an opt-in design is *forgetting the flag*, running a
clean A/B comparison, and losing the data. Always-save inverts the
default so the cost of forgetting is a slightly cluttered `reports/`
instead of lost work.

`--no-save` exists for quick interactive runs where stdout is enough.

## Why the filename scheme

`<ISO timestamp>_<signal>_t<time>_p<pitch>_b<block>_sr<rate>_sweep<rate>.json`

- **ISO timestamp first** so `ls -1 reports/` sorts chronologically.
- **`-` instead of `:`** because Windows / many shells / some FS choke on
  colons in filenames. The timestamp inside the JSON keeps the canonical
  form with colons.
- **Key params in the stem** so you can skim a directory listing without
  opening files. Especially helpful for A/B sweeps where you keep the
  same signal+rates and vary one knob.
- **Not a hash** — short-hash schemes hide the timestamp inside the
  file. Sortability and skimmability beat collision-resistance here;
  timestamps make collisions practically impossible at human pace, and
  `sleep 1` between scripted runs ensures uniqueness.

## What's captured at build-time vs runtime

| Field                             | Where        | Why |
|-----------------------------------|--------------|-----|
| Compiler ID / version / flags     | CMake configure | Stable until next reconfigure; cheaper to bake in than reprobe at runtime. |
| Build type, build timestamp       | CMake configure | Same.                                                            |
| timepitch-bench git short SHA + dirty   | CMake configure | Identifies the bench binary itself. Dirty flag warns reviewers. |
| Library git SHAs + tags           | CMake configure | Resolved by FetchContent at configure time; the binary is linked against those exact trees. Runtime introspection would require each library to expose a version symbol, which not all do. |
| Hostname, uname                   | Runtime      | Cheap; can change between runs of the same binary.              |
| CPU model + flags                 | Runtime      | Same. Also captures whether you ran on perf or efficiency cores via the `cpu_flags` snapshot. |
| Wall-clock timestamp (UTC ISO)    | Runtime      | Has to be runtime by definition.                                |

The CMake side is plumbed via `configure_file(build_info.h.in ...)` into
a generated header included by `report.cpp`. This is preferable to
embedding a generated `.cpp` because the build system already does the
substitution exactly once per configure and the header sits naturally in
the include path.

The git-rev capture uses `git rev-parse --short HEAD` directly (no
`EXISTS .git` precheck), letting git walk up to find the repo. That
matters because `timepitch-bench/` is a sub-tree of the parent MondoLoop repo:
the `.git` lives one directory up.

## Why a separate `commit-reports.sh` script

Considered: a `--commit "msg"` flag on the benchmark binary; a git
post-commit hook; a Makefile target.

- **A flag on the binary** mixes concerns. The benchmark should produce
  data; git wrangling belongs elsewhere. It also makes test iteration
  awkward (you don't want every exploratory run committed).
- **A git hook** would auto-commit on every benchmark run, which the
  always-save design already chose against — exploratory runs would
  pollute history.
- **A Makefile target** would work but the project doesn't have a
  top-level Makefile to attach to, and CMake's `add_custom_target` would
  drag the script into the build artifacts.

A standalone shell script in `scripts/` is the lightest option. It
respects the human-in-the-loop pattern: you run a battery, look at the
results, then commit either all or none with one command. The script
auto-generates commit descriptions from JSON metadata so the message
content lines up exactly with what was measured — no drift between
what's claimed in the commit and what's in the file.

Two modes (`--individual` vs default batch) cover the two real
workflows:

- **Batch commit** for a tied set of runs that belong together (an A/B
  comparison, a parameter sweep).
- **Individual commits** for a series where each run is independently
  significant (e.g. one upstream PR per library tweak).

`--dry-run` exists because git operations are visible to others, so per
the project's care-with-actions stance, the user wants to see what will
land before it lands.

## Why charts read from `reports/` (not from hardcoded numbers)

The first version of `make_charts.py` had numbers baked in. That's fine
for a static document but it means every re-run requires hand-editing
the script — exactly the kind of friction that lets stale figures slip
into the doc unnoticed.

The current version globs `reports/*.json`, picks the most recent run
per `(signal, sweep, time_ratio, pitch_scale)` tuple, and rebuilds the
figures. New reports automatically supersede old ones. Old reports stay
on disk as evidence but don't fight the doc.

`matplotlib` is a docs-only optional dependency, not part of the bench
build. Anyone can regenerate the figures with `pip install --user
matplotlib && python3 docs/make_charts.py`. Anyone who doesn't care
about the figures can ignore it entirely.

## Deliberately not done

- **Cross-run comparison CLI** — diffing report A vs report B with a
  pretty side-by-side. Tempting, but the JSON is structured enough that
  `jq` and `python -c` cover the cases we have today.
- **`reports/` in `.gitignore`** — left untracked but not ignored, so
  `git status` keeps showing them as untracked until the user decides
  what to keep. An ignore rule would hide them and make the commit
  script feel magical.
- **Compression / archival** — reports are small (KBs each). Worry
  about it when there are thousands.
- **Schema versioning beyond `schema_version: 1`** — the field exists
  but no migration tooling is in place. The version bump is the contract
  if we ever need to break compatibility.

## Summary

The flow exists because the project's eventual outputs — library
selection decisions, upstream PRs, Android-vs-desktop perf comparisons —
all rest on benchmark numbers that need to outlive the session that
produced them. Per-run JSON + text mirror, always-save with opt-out,
sortable filenames, full build/host provenance, and a thin commit
script: each piece earns its place against a specific failure mode in
the absence of that piece.
