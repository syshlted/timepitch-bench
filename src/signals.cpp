#include "signals.h"

#include <cmath>
#include <random>
#include <stdexcept>

namespace bench {

SignalKind parse_signal_kind(const std::string& s) {
    if (s == "sine") return SignalKind::Sine;
    if (s == "sweep") return SignalKind::SineSweep;
    if (s == "impulse") return SignalKind::Impulse;
    if (s == "noise") return SignalKind::WhiteNoise;
    if (s == "shepard") return SignalKind::Shepard;
    throw std::runtime_error("unknown signal: " + s);
}

const char* signal_kind_name(SignalKind k) {
    switch (k) {
        case SignalKind::Sine:       return "sine";
        case SignalKind::SineSweep:  return "sweep";
        case SignalKind::Impulse:    return "impulse";
        case SignalKind::WhiteNoise: return "noise";
        case SignalKind::Shepard:    return "shepard";
    }
    return "?";
}

std::vector<float> generate_signal(SignalKind kind, int sample_rate,
                                   int channels, std::size_t frames,
                                   double shepard_sweep_rate) {
    std::vector<float> out(frames * channels, 0.0f);
    const double sr = static_cast<double>(sample_rate);

    switch (kind) {
        case SignalKind::Sine: {
            const double freq = 1000.0;
            const double w = 2.0 * M_PI * freq / sr;
            for (std::size_t i = 0; i < frames; ++i) {
                float s = static_cast<float>(0.5 * std::sin(w * static_cast<double>(i)));
                for (int c = 0; c < channels; ++c) out[i * channels + c] = s;
            }
            break;
        }
        case SignalKind::SineSweep: {
            // log sweep 20 Hz -> sr/2 over the buffer
            const double f0 = 20.0;
            const double f1 = sr * 0.5;
            const double T = static_cast<double>(frames) / sr;
            const double K = T * f0 / std::log(f1 / f0);
            const double L = T / std::log(f1 / f0);
            for (std::size_t i = 0; i < frames; ++i) {
                double t = static_cast<double>(i) / sr;
                double phase = 2.0 * M_PI * K * (std::exp(t / L) - 1.0);
                float s = static_cast<float>(0.5 * std::sin(phase));
                for (int c = 0; c < channels; ++c) out[i * channels + c] = s;
            }
            break;
        }
        case SignalKind::Impulse: {
            // impulse every 100 ms
            const std::size_t period = static_cast<std::size_t>(sr * 0.1);
            for (std::size_t i = 0; i < frames; i += period) {
                for (int c = 0; c < channels; ++c) out[i * channels + c] = 0.9f;
            }
            break;
        }
        case SignalKind::WhiteNoise: {
            std::mt19937 rng(0xC0FFEEu);
            std::uniform_real_distribution<float> dist(-0.5f, 0.5f);
            for (auto& x : out) x = dist(rng);
            break;
        }
        case SignalKind::Shepard: {
            // Continuously rising Shepard tone. Each partial sweeps upward
            // in log-freq at sweep_rate octaves/sec; amplitudes are fixed in
            // log-freq under a Gaussian envelope, so partials fade in at the
            // bottom and out at the top as they sweep through.
            const int    num_partials  = 10;
            const double f_base        = 27.5;            // low A0
            const double center_log2   = std::log2(500.0);// envelope center
            const double sigma_oct     = 2.0;             // envelope width
            const double sweep_rate    = shepard_sweep_rate; // oct/sec, 0 = stationary
            const double nyquist       = sr * 0.5;
            const double ln2           = std::log(2.0);
            const bool   stationary    = std::fabs(sweep_rate) < 1e-9;

            double peak_abs = 0.0;
            for (std::size_t i = 0; i < frames; ++i) {
                double t = static_cast<double>(i) / sr;
                double octave_shift = sweep_rate * t;
                double sample = 0.0;
                for (int k = 0; k < num_partials; ++k) {
                    double f0 = f_base * std::pow(2.0, static_cast<double>(k));
                    double f  = stationary ? f0 : f0 * std::pow(2.0, octave_shift);
                    if (f < 20.0 || f > nyquist) continue;
                    // Stationary: phase = 2π f0 t
                    // Sweeping:   phase = 2π f0 (2^(rate t) - 1) / (rate · ln2)
                    double phase = stationary
                        ? 2.0 * M_PI * f0 * t
                        : 2.0 * M_PI * f0 *
                          (std::pow(2.0, octave_shift) - 1.0) /
                          (sweep_rate * ln2);
                    double dz  = (std::log2(f) - center_log2) / sigma_oct;
                    double amp = std::exp(-0.5 * dz * dz);
                    sample += amp * std::sin(phase);
                }
                if (std::fabs(sample) > peak_abs) peak_abs = std::fabs(sample);
                float s = static_cast<float>(sample);
                for (int c = 0; c < channels; ++c) out[i * channels + c] = s;
            }
            // Normalize to ~0.5 peak so downstream stretchers see a sane level.
            if (peak_abs > 0.0) {
                float g = static_cast<float>(0.5 / peak_abs);
                for (auto& x : out) x *= g;
            }
            break;
        }
    }
    return out;
}

} // namespace bench
