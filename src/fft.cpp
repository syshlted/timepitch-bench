#include "fft.h"

#include <algorithm>
#include <cmath>

namespace bench {

void fft(std::vector<std::complex<double>>& x) {
    const std::size_t N = x.size();
    if (N <= 1) return;

    // Bit-reverse permutation.
    for (std::size_t i = 1, j = 0; i < N; ++i) {
        std::size_t bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(x[i], x[j]);
    }

    for (std::size_t len = 2; len <= N; len <<= 1) {
        double ang = -2.0 * M_PI / static_cast<double>(len);
        std::complex<double> wlen(std::cos(ang), std::sin(ang));
        for (std::size_t i = 0; i < N; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (std::size_t k = 0; k < len / 2; ++k) {
                auto u = x[i + k];
                auto v = x[i + k + len / 2] * w;
                x[i + k] = u + v;
                x[i + k + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

static std::size_t next_pow2(std::size_t n) {
    std::size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

std::vector<double> magnitude_spectrum(const std::vector<float>& mono,
                                       std::size_t fft_size) {
    const std::size_t N = next_pow2(std::min(mono.size(), fft_size));
    std::vector<std::complex<double>> buf(N, {0.0, 0.0});
    // Hann window for cleaner peak picking.
    for (std::size_t i = 0; i < N && i < mono.size(); ++i) {
        double w = 0.5 * (1.0 - std::cos(2.0 * M_PI * static_cast<double>(i) /
                                         static_cast<double>(N - 1)));
        buf[i] = { static_cast<double>(mono[i]) * w, 0.0 };
    }
    fft(buf);
    std::vector<double> mag(N / 2 + 1);
    for (std::size_t i = 0; i < mag.size(); ++i) mag[i] = std::abs(buf[i]);
    return mag;
}

double estimate_fundamental_hz(const std::vector<float>& mono,
                               int sample_rate, std::size_t fft_size) {
    auto mag = magnitude_spectrum(mono, fft_size);
    if (mag.size() < 3) return 0.0;

    // Skip DC and the first couple of bins; find peak.
    std::size_t peak = 2;
    for (std::size_t i = 2; i < mag.size() - 1; ++i) {
        if (mag[i] > mag[peak]) peak = i;
    }

    // Parabolic interpolation around the peak.
    double y0 = mag[peak - 1], y1 = mag[peak], y2 = mag[peak + 1];
    double denom = (y0 - 2.0 * y1 + y2);
    double offset = denom != 0.0 ? 0.5 * (y0 - y2) / denom : 0.0;
    double bin = static_cast<double>(peak) + offset;

    const std::size_t N = next_pow2(std::min(mono.size(), fft_size));
    return bin * static_cast<double>(sample_rate) / static_cast<double>(N);
}

} // namespace bench
