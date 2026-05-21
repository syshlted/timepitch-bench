#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace bench {

struct CpuTiming {
    double total_wall_seconds = 0.0;
    double total_cpu_seconds  = 0.0;
    std::size_t calls         = 0;
    std::size_t input_frames  = 0;
    std::size_t output_frames = 0;

    // Real-time factor: how many seconds of audio processed per CPU second.
    // >1 means faster than real-time. Computed against output_frames.
    double real_time_factor(int sample_rate) const;
    double mean_wall_us_per_call() const;
    double p95_wall_us_per_call() const;
    void record_call(double wall_us);

    std::vector<double> per_call_wall_us;
};

// Peak resident set size in kilobytes, sampled now. Linux only; returns 0
// elsewhere.
std::size_t peak_rss_kb();

struct QualityReport {
    // For pure-tone inputs: detected output fundamental vs expected.
    double expected_hz = 0.0;
    double detected_hz = 0.0;
    double pitch_error_cents = 0.0;

    // Spectral SNR vs an ideal reference, in dB (higher = closer to ideal).
    // -INF if reference comparison wasn't applicable.
    double spectral_snr_db = 0.0;
};

// Compute pitch error (in cents) given expected and detected Hz.
double cents_between(double expected_hz, double detected_hz);

// Compare two real signals via FFT magnitude; returns 10*log10(ref_pow/err_pow).
double spectral_snr_db(const std::vector<float>& reference_mono,
                       const std::vector<float>& candidate_mono,
                       std::size_t fft_size);

} // namespace bench
