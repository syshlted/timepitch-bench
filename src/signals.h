#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace bench {

enum class SignalKind {
    Sine,        // single tone at 1 kHz
    SineSweep,   // log sweep 20 Hz -> Nyquist
    Impulse,     // single sample impulse train
    WhiteNoise,  // uniform white noise
    Shepard,     // octave-spaced partials sweeping under a fixed Gaussian
                 // envelope in log-freq — exercises a wide spectrum at
                 // once with a known, easy-to-verify structure.
};

SignalKind parse_signal_kind(const std::string& s);
const char* signal_kind_name(SignalKind k);

// Generates an interleaved stereo float buffer at the requested sample rate.
// Length is given in frames. `shepard_sweep_rate` (octaves/sec) only affects
// the Shepard signal; 0 means stationary partials.
std::vector<float> generate_signal(SignalKind kind, int sample_rate,
                                   int channels, std::size_t frames,
                                   double shepard_sweep_rate = 0.5);

} // namespace bench
