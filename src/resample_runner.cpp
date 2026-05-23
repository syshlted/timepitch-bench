#include "resample_runner.h"

#include "fft.h"
#include "metrics.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
#include <sstream>

namespace bench {

const char* resample_signal_name(ResampleSignal s) {
    switch (s) {
        case ResampleSignal::Sine:          return "sine";
        case ResampleSignal::SineNearSrcNy: return "sine-near-src-nyquist";
        case ResampleSignal::Impulse:       return "impulse";
        case ResampleSignal::Sweep:         return "sweep";
    }
    return "?";
}

ResampleSignal parse_resample_signal(const std::string& s) {
    if (s == "sine")               return ResampleSignal::Sine;
    if (s == "sine-near-src-nyquist" || s == "alias") return ResampleSignal::SineNearSrcNy;
    if (s == "impulse")            return ResampleSignal::Impulse;
    if (s == "sweep")              return ResampleSignal::Sweep;
    return ResampleSignal::Sine;
}

namespace {

std::vector<float> make_sine(int sample_rate, std::size_t frames,
                             double freq_hz, double amp = 0.5) {
    std::vector<float> v(frames);
    const double tw = 2.0 * M_PI * freq_hz / sample_rate;
    for (std::size_t i = 0; i < frames; ++i) {
        v[i] = static_cast<float>(amp * std::sin(tw * static_cast<double>(i)));
    }
    return v;
}

std::vector<float> make_impulse(std::size_t frames, std::size_t at) {
    std::vector<float> v(frames, 0.0f);
    if (at < frames) v[at] = 1.0f;
    return v;
}

// Log sweep 20 Hz -> stop_hz over the buffer's duration.
std::vector<float> make_sweep(int sample_rate, std::size_t frames,
                              double stop_hz, double amp = 0.5) {
    std::vector<float> v(frames);
    const double f0 = 20.0;
    const double T = static_cast<double>(frames) / sample_rate;
    const double k = std::log(stop_hz / f0) / T;
    double phase = 0.0;
    const double dt = 1.0 / sample_rate;
    for (std::size_t i = 0; i < frames; ++i) {
        double t = i * dt;
        double f = f0 * std::exp(k * t);
        phase += 2.0 * M_PI * f * dt;
        v[i] = static_cast<float>(amp * std::sin(phase));
    }
    return v;
}

// Interleave a mono vector into N-channel output (replicate).
std::vector<float> interleave(const std::vector<float>& mono, int channels) {
    if (channels == 1) return mono;
    std::vector<float> v(mono.size() * channels);
    for (std::size_t i = 0; i < mono.size(); ++i) {
        for (int c = 0; c < channels; ++c) v[i * channels + c] = mono[i];
    }
    return v;
}

std::vector<float> deinterleave_first_channel(const float* buf,
                                              std::size_t frames,
                                              int channels) {
    std::vector<float> v(frames);
    for (std::size_t i = 0; i < frames; ++i) v[i] = buf[i * channels];
    return v;
}

double rms_dbfs(const std::vector<float>& x) {
    if (x.empty()) return -std::numeric_limits<double>::infinity();
    double s = 0.0;
    for (float v : x) s += static_cast<double>(v) * v;
    double rms = std::sqrt(s / x.size());
    if (rms <= 1e-30) return -300.0;
    return 20.0 * std::log10(rms);
}

double peak_dbfs(const std::vector<float>& x) {
    double m = 0.0;
    for (float v : x) m = std::max(m, std::fabs(static_cast<double>(v)));
    if (m <= 1e-30) return -300.0;
    return 20.0 * std::log10(m);
}

// Locate the largest |x| in the buffer; report its index.
std::size_t argmax_abs(const std::vector<float>& x) {
    std::size_t arg = 0;
    double m = 0.0;
    for (std::size_t i = 0; i < x.size(); ++i) {
        double a = std::fabs(static_cast<double>(x[i]));
        if (a > m) { m = a; arg = i; }
    }
    return arg;
}

} // namespace

ResampleResult run_one_resample(Resampler& r, const ResampleRunOptions& opts) {
    ResampleResult res;
    res.resampler_name = r.name();
    res.opts = opts;

    const std::size_t in_frames =
        static_cast<std::size_t>(opts.duration_seconds * opts.src_rate);
    const std::size_t max_out_frames =
        static_cast<std::size_t>(in_frames * (double)opts.dst_rate / opts.src_rate) + 1024;

    const double dst_nyquist = 0.5 * opts.dst_rate;
    const double src_nyquist = 0.5 * opts.src_rate;

    // Build mono input per signal type.
    std::vector<float> in_mono;
    double sine_freq = opts.sine_hz;
    double alias_freq = 0.95 * src_nyquist;

    switch (opts.signal) {
        case ResampleSignal::Sine:
            in_mono = make_sine(opts.src_rate, in_frames, sine_freq);
            break;
        case ResampleSignal::SineNearSrcNy:
            // Only meaningful on downsampling: pick a frequency above dst
            // Nyquist (so it must be filtered out) and below src Nyquist
            // (so it's representable in the source). On upsampling, leave
            // alias_freq at -1 to flag n/a.
            if (src_nyquist > dst_nyquist) {
                alias_freq = std::min(0.95 * src_nyquist,
                                      dst_nyquist + 0.4 * (src_nyquist - dst_nyquist));
                in_mono = make_sine(opts.src_rate, in_frames, alias_freq);
            } else {
                alias_freq = -1.0;
                in_mono.assign(in_frames, 0.0f); // silence; we'll skip metric
            }
            break;
        case ResampleSignal::Impulse:
            in_mono = make_impulse(in_frames, in_frames / 2);
            break;
        case ResampleSignal::Sweep:
            in_mono = make_sweep(opts.src_rate, in_frames, 0.95 * src_nyquist);
            break;
    }

    auto in_interleaved = interleave(in_mono, opts.channels);
    std::vector<float> out_interleaved(max_out_frames * opts.channels);

    auto t0 = std::chrono::steady_clock::now();
    std::size_t out_frames = r.process(
        ResampleConfig{opts.src_rate, opts.dst_rate, opts.channels},
        in_interleaved.data(), in_frames,
        out_interleaved.data(), max_out_frames);
    auto t1 = std::chrono::steady_clock::now();

    res.wall_seconds = std::chrono::duration<double>(t1 - t0).count();
    res.in_frames  = in_frames;
    res.out_frames = out_frames;
    if (out_frames == 0) return res;

    auto out_mono = deinterleave_first_channel(out_interleaved.data(),
                                               out_frames, opts.channels);

    // Trim 5% from each end to suppress filter startup/tail edges.
    const std::size_t trim = std::max<std::size_t>(out_mono.size() / 20, 64);
    if (out_mono.size() > 2 * trim) {
        out_mono.erase(out_mono.begin(), out_mono.begin() + trim);
        out_mono.resize(out_mono.size() - trim);
    }

    switch (opts.signal) {
        case ResampleSignal::Sine: {
            auto ref = make_sine(opts.dst_rate, out_mono.size(), sine_freq);
            res.in_band_snr_db = spectral_snr_db(ref, out_mono, opts.fft_size);
            break;
        }
        case ResampleSignal::SineNearSrcNy: {
            res.alias_input_freq_hz = alias_freq;
            if (alias_freq > 0.0) {
                // Downsampling: input is above dst Nyquist, output should be
                // silence. RMS = leakage; lower is better rejection.
                res.alias_residual_dbfs = rms_dbfs(out_mono);
            } else {
                // Upsampling: aliasing test does not apply.
                res.alias_residual_dbfs = std::numeric_limits<double>::quiet_NaN();
            }
            break;
        }
        case ResampleSignal::Impulse: {
            // Find the peak; measure -60 dB envelope width on each side.
            std::size_t peak = argmax_abs(out_mono);
            res.impulse_peak_dbfs = peak_dbfs({out_mono[peak]});
            const double thresh = std::pow(10.0, -60.0 / 20.0) *
                                  std::fabs((double)out_mono[peak]);
            int pre = 0;
            for (std::size_t i = peak; i-- > 0;) {
                if (std::fabs((double)out_mono[i]) > thresh) ++pre;
                else if (pre > 0 && (peak - i) > 8 &&
                         std::fabs((double)out_mono[i]) <= thresh) break;
            }
            int post = 0;
            for (std::size_t i = peak + 1; i < out_mono.size(); ++i) {
                if (std::fabs((double)out_mono[i]) > thresh) ++post;
                else if (post > 0 && (i - peak) > 8 &&
                         std::fabs((double)out_mono[i]) <= thresh) break;
            }
            res.impulse_preringing_samples = pre;
            res.impulse_postringing_samples = post;
            break;
        }
        case ResampleSignal::Sweep: {
            // Reference: the same log sweep generated directly at dst_rate
            // and truncated to the same band-limit. spectral_snr_db then
            // captures the broadband fidelity of the conversion.
            const double stop = std::min(0.95 * src_nyquist, 0.95 * dst_nyquist);
            auto ref = make_sweep(opts.dst_rate, out_mono.size(), stop);
            res.sweep_passband_ripple_db = spectral_snr_db(ref, out_mono, opts.fft_size);
            break;
        }
    }
    return res;
}

std::string format_resample_result(const ResampleResult& r) {
    std::ostringstream os;
    os << r.resampler_name << "  "
       << r.opts.src_rate << " -> " << r.opts.dst_rate << "  "
       << resample_signal_name(r.opts.signal) << "  ";
    switch (r.opts.signal) {
        case ResampleSignal::Sine:
            os << "in-band SNR = "
               << (std::isinf(r.in_band_snr_db) ? std::string("inf")
                   : std::to_string(r.in_band_snr_db)) << " dB"
               << "   @ " << r.opts.sine_hz << " Hz";
            break;
        case ResampleSignal::SineNearSrcNy:
            if (std::isnan(r.alias_residual_dbfs)) {
                os << "alias residual = n/a (upsampling has no out-of-band content to reject)";
            } else {
                os << "alias residual = " << r.alias_residual_dbfs << " dBFS"
                   << "   (input @ " << r.alias_input_freq_hz << " Hz; dst Ny = "
                   << (0.5 * r.opts.dst_rate) << " Hz)";
            }
            break;
        case ResampleSignal::Impulse:
            os << "impulse: pre = " << r.impulse_preringing_samples
               << " smp, post = " << r.impulse_postringing_samples
               << " smp (-60 dB)";
            break;
        case ResampleSignal::Sweep:
            // The field is overloaded — now holds spectral SNR vs an
            // analytically-regenerated sweep at dst_rate. Higher = better.
            os << "broadband SNR = " << r.sweep_passband_ripple_db << " dB";
            break;
    }
    os << "   [wall " << r.wall_seconds << " s, "
       << r.in_frames << " -> " << r.out_frames << " smp]";
    return os.str();
}

void print_resample_result(const ResampleResult& r) {
    std::printf("%s\n", format_resample_result(r).c_str());
}

} // namespace bench
