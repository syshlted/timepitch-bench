# Resampler bench — first results (2026-05-23)

Context: import-path resampler for `.mloop` (D-016). The import path is
**offline**, so we run both libraries at their highest documented quality
tier. The question is whether either is good enough — and if both are, whether
one obviously beats the other on the metrics we can measure synthetically.

## Setup

- Libraries: **libsamplerate 0.2.2** at `SRC_SINC_BEST_QUALITY`,
  **r8brain-free-src v6.5** via `CDSPResampler24` (defaults — 158 dB
  attenuation, transition band 2.0).
- Host: x86_64 Linux desktop, GCC 15 release build.
- Input signals: 2-second mono float buffers.
- Trim 5% from each end of the output before measurement to suppress filter
  startup/tail edges.

Runner: `timepitch_bench --resample` (see [src/resample_runner.cpp](../src/resample_runner.cpp)).

## Numbers

### In-band 1 kHz sine — spectral SNR vs analytical reference (dB)

| src → dst (Hz) | libsamplerate | r8brain |
|---------------:|--------------:|--------:|
| 22050 → 48000  | 149.0 | 150.0 |
| 44100 → 48000  | 148.3 | 150.3 |
| 88200 → 48000  | 140.5 | 151.6 |
| 96000 → 48000  | 277.3 | 259.8 |

The 96k→48k row is essentially at float-precision floor — 2:1 ratio with
no fractional interpolation needed. Everywhere else, both libraries deliver
~150 dB SNR, comfortably below the 24-bit theoretical floor.

### Alias rejection (downsample only) — output RMS in dBFS

Input: a sine at ~`0.4 * src_nyquist + 0.6 * dst_nyquist` Hz (above dst
Nyquist, below src Nyquist). Any output is leakage.

| src → dst (Hz) | input (Hz) | libsamplerate | r8brain |
|---------------:|-----------:|--------------:|--------:|
| 88200 → 48000  | 32040 | -161.6 dBFS | -164.7 dBFS |
| 96000 → 48000  | 33600 | -158.1 dBFS | -173.3 dBFS |

Both reject aliasing well below any audible threshold. r8brain has a small
edge (~10 dB) at its top tier.

### Impulse smear — samples between peak and -60 dB envelope crossing

| src → dst (Hz) | libsamplerate | r8brain |
|---------------:|--------------:|--------:|
| 22050 → 48000  | 78 / 78  | 30 / 30 |
| 44100 → 48000  | 43 / 43  | 30 / 30 |
| 88200 → 48000  | 28 / 28  | 52 / 52 |
| 96000 → 48000  | 28 / 28  | 52 / 52 |

Pre / post are symmetric for both (both use linear-phase FIRs). 50 samples
at 48 kHz ≈ 1 ms — neither library produces perceptually relevant smear.
**Interestingly, the ringing length doesn't covary cleanly between the
two**: r8brain rings less on upsample, more on downsample; libsamplerate
the reverse.

### Throughput (offline wall time, 2-sec input)

| src → dst (Hz) | libsamplerate | r8brain | speedup |
|---------------:|--------------:|--------:|--------:|
| 22050 → 48000  | 0.085 s | 0.0023 s | 37× |
| 44100 → 48000  | 0.085 s | 0.0034 s | 25× |
| 88200 → 48000  | 0.147 s | 0.0056 s | 26× |
| 96000 → 48000  | 0.103 s | 0.0034 s | 30× |

r8brain is dramatically faster, despite both being marketed as
high-quality offline tiers. Likely the algorithmic constant (FIR length
× polyphase branches) is smaller in r8brain for equivalent SNR targets.

## Reading the numbers

- **Both libraries are objectively excellent.** Either would be a defensible
  choice for the import path. SNR, alias rejection, and impulse smear are
  all well below perceptual relevance.
- **The synthetic metrics don't pick a winner.** r8brain is faster and
  rejects aliasing slightly better; libsr has shorter impulse smear on the
  important downsample case. Neither difference is audible.
- **CPU isn't load-bearing for import** (a 3-min loop import takes
  fractions of a second either way), but it might matter on lower-end
  Android devices if we ever batch-import a folder. r8brain's headroom
  there is real.
- **Licensing**: libsamplerate is BSD-2-Clause, r8brain is MIT. Both are
  unambiguously fine for static linking on iOS.

## What this doesn't tell us

- **Listening.** Both pass synthetic tests; the deciding test is whether
  real music sounds different after each. Defer to the next session.
- **Behaviour on transient-rich material** (drums, percussion). The
  impulse test is a proxy; real percussive content can probe pre-ring
  audibility differently.
- **Compressed-format decode artifacts.** Once the import path includes
  an MP3/AAC decoder, the resampler is no longer the dominant artefact
  source. Worth re-comparing in the full chain.

## Decision

**Picked: r8brain-free** (D-017, 2026-05-23). Synthetic quality is a tie at
audibly-irrelevant levels; the CPU delta is real and one-sided. Listening
test is now an optional post-integration sanity check rather than a gate.

libsamplerate stays wired in `timepitch-bench/` as a permanent comparison point
(same pattern as keeping SoundTouch / Rubber Band post-D-012). Not vendored
into production.
