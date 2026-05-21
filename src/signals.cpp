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
    throw std::runtime_error("unknown signal: " + s);
}

const char* signal_kind_name(SignalKind k) {
    switch (k) {
        case SignalKind::Sine:       return "sine";
        case SignalKind::SineSweep:  return "sweep";
        case SignalKind::Impulse:    return "impulse";
        case SignalKind::WhiteNoise: return "noise";
    }
    return "?";
}

std::vector<float> generate_signal(SignalKind kind, int sample_rate,
                                   int channels, std::size_t frames) {
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
    }
    return out;
}

} // namespace bench
