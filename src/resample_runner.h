#pragma once

#include "resampler.h"

#include <string>

namespace bench {

enum class ResampleSignal {
    Sine,          // single sine well within dst Nyquist; tests in-band SNR
    SineNearSrcNy, // sine near src Nyquist (above dst Nyquist on downsample);
                   // tests alias rejection of the anti-imaging filter
    Impulse,       // single-sample impulse; tests transient smear
    Sweep,         // log sweep 20 Hz -> dst_nyquist*0.95
};

const char* resample_signal_name(ResampleSignal s);
ResampleSignal parse_resample_signal(const std::string& s);

struct ResampleRunOptions {
    int src_rate = 44100;
    int dst_rate = 48000;
    int channels = 1;
    double duration_seconds = 2.0;
    ResampleSignal signal = ResampleSignal::Sine;
    double sine_hz = 1000.0;     // for Sine
    std::size_t fft_size = 16384;
};

struct ResampleResult {
    std::string resampler_name;
    ResampleRunOptions opts;

    // Timing
    double wall_seconds = 0.0;
    std::size_t in_frames = 0;
    std::size_t out_frames = 0;

    // Sine: spectral SNR vs analytical reference at dst_rate (dB).
    double in_band_snr_db = 0.0;

    // SineNearSrcNy: total output RMS in dBFS — lower = better alias rejection.
    // The "signal" frequency lives above dst Nyquist, so any output is leakage.
    double alias_residual_dbfs = 0.0;
    double alias_input_freq_hz = 0.0;

    // Impulse: number of output samples between the peak and where the
    // envelope first crosses -60 dB on each side (pre-/post-ring).
    int impulse_preringing_samples = 0;
    int impulse_postringing_samples = 0;
    double impulse_peak_dbfs = 0.0;

    // Sweep: spectral flatness of |H| from 20 Hz to 0.9 * dst_nyquist (dB).
    // Just peak-to-trough; not a substitute for proper passband ripple.
    double sweep_passband_ripple_db = 0.0;
};

ResampleResult run_one_resample(Resampler& r, const ResampleRunOptions& opts);

std::string format_resample_result(const ResampleResult& r);
void print_resample_result(const ResampleResult& r);

} // namespace bench
