#include "runner.h"
#include "fft.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace bench {

// Least-squares fit of y = A + B*x + C*x^2 to n samples. Returns false if
// the system is singular or n < 3.
static bool fit_quadratic(const std::vector<double>& xs,
                          const std::vector<double>& ys,
                          double& A, double& B, double& C) {
    const std::size_t n = xs.size();
    if (n < 3 || ys.size() != n) return false;
    double S0 = static_cast<double>(n), S1 = 0, S2 = 0, S3 = 0, S4 = 0;
    double T0 = 0, T1 = 0, T2 = 0;
    for (std::size_t i = 0; i < n; ++i) {
        double x = xs[i], y = ys[i];
        double x2 = x * x;
        S1 += x; S2 += x2; S3 += x2 * x; S4 += x2 * x2;
        T0 += y; T1 += y * x; T2 += y * x2;
    }
    // 3x3 system: [S0 S1 S2; S1 S2 S3; S2 S3 S4] [A;B;C] = [T0;T1;T2]
    double m[3][4] = {
        {S0, S1, S2, T0},
        {S1, S2, S3, T1},
        {S2, S3, S4, T2},
    };
    for (int i = 0; i < 3; ++i) {
        int piv = i;
        for (int r = i + 1; r < 3; ++r)
            if (std::fabs(m[r][i]) > std::fabs(m[piv][i])) piv = r;
        if (std::fabs(m[piv][i]) < 1e-12) return false;
        if (piv != i) for (int c = 0; c < 4; ++c) std::swap(m[i][c], m[piv][c]);
        for (int r = 0; r < 3; ++r) {
            if (r == i) continue;
            double f = m[r][i] / m[i][i];
            for (int c = i; c < 4; ++c) m[r][c] -= f * m[i][c];
        }
    }
    A = m[0][3] / m[0][0];
    B = m[1][3] / m[1][1];
    C = m[2][3] / m[2][2];
    return true;
}

// Fit a Gaussian envelope to spectral peaks: log(magnitude) = A + B*x + C*x^2
// where x = log2(freq). Returns mu (Hz) and sigma (octaves) via outputs.
static bool fit_gaussian_envelope(const std::vector<SpectralPeak>& peaks,
                                  double& mu_hz, double& sigma_oct) {
    std::vector<double> xs, ys;
    for (auto& p : peaks) {
        if (p.magnitude <= 0.0) continue;
        xs.push_back(std::log2(p.frequency_hz));
        ys.push_back(std::log(p.magnitude));
    }
    double A_, B_, C_;
    if (!fit_quadratic(xs, ys, A_, B_, C_)) return false;
    if (C_ >= 0.0) return false;
    mu_hz     = std::pow(2.0, -B_ / (2.0 * C_));
    sigma_oct = std::sqrt(-1.0 / (2.0 * C_));
    return true;
}

// Pull one channel out of an interleaved stereo buffer.
static std::vector<float> take_left(const std::vector<float>& interleaved,
                                    int channels) {
    std::vector<float> out;
    out.reserve(interleaved.size() / channels);
    for (std::size_t i = 0; i < interleaved.size(); i += channels) {
        out.push_back(interleaved[i]);
    }
    return out;
}

RunResult run_one(Stretcher& s, const RunOptions& opts) {
    RunResult r;
    r.stretcher_name = s.name();
    r.opts = opts;

    StretchConfig cfg;
    cfg.sample_rate = opts.sample_rate;
    cfg.channels = opts.channels;
    cfg.time_ratio = opts.time_ratio;
    cfg.pitch_scale = opts.pitch_scale;
    cfg.block_size = opts.block_size;

    if (!s.init(cfg)) {
        std::fprintf(stderr, "init failed for %s\n", s.name());
        return r;
    }

    const std::size_t total_in_frames =
        static_cast<std::size_t>(opts.duration_seconds * opts.sample_rate);

    auto input = generate_signal(opts.signal, opts.sample_rate, opts.channels,
                                 total_in_frames, opts.shepard_sweep_rate);

    // Output buffer sized generously for the worst-case stretch ratio plus
    // tail. The algorithm may return less.
    const std::size_t out_capacity =
        static_cast<std::size_t>(total_in_frames * std::max(2.0, opts.time_ratio * 2.0));
    std::vector<float> output(out_capacity * opts.channels, 0.0f);

    std::vector<float> out_block(opts.block_size * opts.channels * 4, 0.0f);

    std::size_t in_pos = 0, out_pos_frames = 0;
    while (in_pos < total_in_frames) {
        std::size_t block_frames =
            std::min<std::size_t>(opts.block_size, total_in_frames - in_pos);

        std::size_t produced = 0;
        auto t0 = std::chrono::steady_clock::now();
        s.process(input.data() + in_pos * opts.channels, block_frames,
                  out_block.data(),
                  out_block.size() / opts.channels,
                  produced);
        auto t1 = std::chrono::steady_clock::now();

        double wall_us =
            std::chrono::duration<double, std::micro>(t1 - t0).count();
        r.timing.record_call(wall_us);
        r.timing.total_wall_seconds += wall_us * 1e-6;
        r.timing.total_cpu_seconds  += wall_us * 1e-6;  // approx; same clock here
        r.timing.input_frames  += block_frames;
        r.timing.output_frames += produced;

        if (produced > 0) {
            std::size_t copy_frames =
                std::min(produced, out_capacity - out_pos_frames);
            std::memcpy(output.data() + out_pos_frames * opts.channels,
                        out_block.data(),
                        copy_frames * opts.channels * sizeof(float));
            out_pos_frames += copy_frames;
        }
        in_pos += block_frames;
    }

    output.resize(out_pos_frames * opts.channels);

    r.peak_rss_kb_after = peak_rss_kb();
    r.reported_latency_frames = s.latency_frames();

    // Quality scoring for tonal inputs: detect output fundamental.
    if (opts.signal == SignalKind::Sine) {
        const double input_hz = 1000.0;
        const double expected_hz = input_hz * opts.pitch_scale;
        auto mono = take_left(output, opts.channels);
        double detected = estimate_fundamental_hz(mono, opts.sample_rate,
                                                  opts.fft_size);
        r.quality.expected_hz = expected_hz;
        r.quality.detected_hz = detected;
        r.quality.pitch_error_cents = cents_between(expected_hz, detected);
    } else if (opts.signal == SignalKind::Shepard) {
        // Take a mid-buffer window of input and output (where stretcher
        // latency tails are irrelevant) and compare peak structure.
        const std::size_t win = 8192;
        auto mono_in  = take_left(input,  opts.channels);
        auto mono_out = take_left(output, opts.channels);

        auto slice_mid = [&](const std::vector<float>& v) {
            std::vector<float> w;
            if (v.size() < win) { w = v; return w; }
            std::size_t start = (v.size() - win) / 2;
            w.assign(v.begin() + start, v.begin() + start + win);
            return w;
        };

        auto in_win  = slice_mid(mono_in);
        auto out_win = slice_mid(mono_out);

        auto in_peaks  = find_top_peaks(in_win,  opts.sample_rate,
                                        opts.fft_size, 8);
        auto out_peaks = find_top_peaks(out_win, opts.sample_rate,
                                        opts.fft_size, 8);

        for (auto& p : in_peaks)  r.quality.shepard_input_peaks_hz.push_back(p.frequency_hz);
        for (auto& p : out_peaks) r.quality.shepard_output_peaks_hz.push_back(p.frequency_hz);

        // Adjacent-octave ratio in output: median of f[i+1]/f[i].
        if (out_peaks.size() >= 2) {
            std::vector<double> ratios;
            for (std::size_t i = 1; i < out_peaks.size(); ++i) {
                ratios.push_back(out_peaks[i].frequency_hz /
                                 out_peaks[i - 1].frequency_hz);
            }
            std::sort(ratios.begin(), ratios.end());
            r.quality.shepard_median_octave_ratio = ratios[ratios.size() / 2];
        }

        // Gaussian envelope fit on both input and output peaks. Comparing
        // output-vs-input cancels the analysis-side window/peak-amplitude
        // bias that affects both equally.
        double mu, sig;
        if (out_peaks.size() >= 3 && fit_gaussian_envelope(out_peaks, mu, sig)) {
            r.quality.shepard_envelope_center_hz = mu;
            r.quality.shepard_envelope_sigma_oct = sig;
            r.quality.shepard_envelope_fit_ok    = true;
        }
        if (in_peaks.size() >= 3 && fit_gaussian_envelope(in_peaks, mu, sig)) {
            r.quality.shepard_input_envelope_center_hz = mu;
            r.quality.shepard_input_envelope_sigma_oct = sig;
            r.quality.shepard_input_envelope_fit_ok    = true;
        }

        // Observed pitch ratio: for each output peak, find nearest input peak
        // in log-freq; take median of out/in ratio.
        if (!out_peaks.empty() && !in_peaks.empty()) {
            std::vector<double> pr;
            for (auto& op : out_peaks) {
                double best_ratio = 0.0;
                double best_log_dist = 1e9;
                for (auto& ip : in_peaks) {
                    double r_ = op.frequency_hz / ip.frequency_hz;
                    double d  = std::fabs(std::log2(r_));
                    if (d < best_log_dist) { best_log_dist = d; best_ratio = r_; }
                }
                pr.push_back(best_ratio);
            }
            std::sort(pr.begin(), pr.end());
            r.quality.shepard_observed_pitch_ratio = pr[pr.size() / 2];
        }
    }

    return r;
}

std::string format_result(const RunResult& r) {
    char buf[512];
    std::string o;
    auto add = [&](const char* fmt, auto... args) {
        std::snprintf(buf, sizeof(buf), fmt, args...);
        o += buf;
    };
    add("=== %s ===\n", r.stretcher_name.c_str());
    add("  signal=%s time_ratio=%.3f pitch_scale=%.3f block=%d\n",
        signal_kind_name(r.opts.signal),
        r.opts.time_ratio, r.opts.pitch_scale, r.opts.block_size);
    add("  input_frames=%zu output_frames=%zu calls=%zu\n",
        r.timing.input_frames, r.timing.output_frames, r.timing.calls);
    add("  wall_total=%.3f ms  mean/call=%.2f us  p95/call=%.2f us\n",
        r.timing.total_wall_seconds * 1000.0,
        r.timing.mean_wall_us_per_call(),
        r.timing.p95_wall_us_per_call());
    add("  realtime_factor=%.2fx  (>1 = faster than realtime)\n",
        r.timing.real_time_factor(r.opts.sample_rate));
    add("  peak_rss=%zu KB\n", r.peak_rss_kb_after);
    if (r.opts.measure_latency) {
        add("  reported_latency=%d frames\n", r.reported_latency_frames);
    }
    if (r.opts.signal == SignalKind::Sine) {
        add("  expected=%.2f Hz  detected=%.2f Hz  error=%.2f cents\n",
            r.quality.expected_hz, r.quality.detected_hz,
            r.quality.pitch_error_cents);
    }
    if (r.opts.signal == SignalKind::Shepard) {
        o += "  input_peaks_hz= ";
        for (double f : r.quality.shepard_input_peaks_hz) add("%.1f ", f);
        o += "\n  output_peaks_hz=";
        for (double f : r.quality.shepard_output_peaks_hz) add("%.1f ", f);
        o += "\n";
        add("  median_octave_ratio=%.4f (expect ~2.0)\n",
            r.quality.shepard_median_octave_ratio);
        add("  observed_pitch_ratio=%.4f (expect ~%.4f)\n",
            r.quality.shepard_observed_pitch_ratio,
            r.opts.pitch_scale);
        if (r.quality.shepard_envelope_fit_ok) {
            double expected_center = 500.0 * r.opts.pitch_scale;
            add("  envelope_center=%.1f Hz (expect ~%.1f)  "
                "envelope_sigma=%.3f oct (expect ~2.000)\n",
                r.quality.shepard_envelope_center_hz,
                expected_center,
                r.quality.shepard_envelope_sigma_oct);
        } else {
            o += "  envelope fit: insufficient data\n";
        }
        if (r.quality.shepard_envelope_fit_ok &&
            r.quality.shepard_input_envelope_fit_ok) {
            double in_mu     = r.quality.shepard_input_envelope_center_hz;
            double out_mu    = r.quality.shepard_envelope_center_hz;
            double in_sigma  = r.quality.shepard_input_envelope_sigma_oct;
            double out_sigma = r.quality.shepard_envelope_sigma_oct;
            double observed_shift_oct = std::log2(out_mu / in_mu);
            double expected_shift_oct = std::log2(r.opts.pitch_scale);
            double center_err_cents = (observed_shift_oct - expected_shift_oct)
                                      * 1200.0;
            double sigma_err = out_sigma - in_sigma;
            add("  envelope_vs_input: center_err=%+.1f cents  "
                "sigma_err=%+.4f oct\n",
                center_err_cents, sigma_err);
        }
    }
    o += "\n";
    return o;
}

void print_result(const RunResult& r) {
    std::fputs(format_result(r).c_str(), stdout);
}

} // namespace bench
