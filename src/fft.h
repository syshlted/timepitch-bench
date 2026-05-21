#pragma once

#include <complex>
#include <cstddef>
#include <vector>

namespace bench {

// Minimal radix-2 Cooley-Tukey FFT, in-place. Size must be a power of two.
// Not optimized — used only for offline quality analysis, not in the hot loop.
void fft(std::vector<std::complex<double>>& x);

// Forward magnitude spectrum of a real signal. Returns size/2+1 bins.
std::vector<double> magnitude_spectrum(const std::vector<float>& mono_signal,
                                       std::size_t fft_size);

// Estimate fundamental frequency in Hz using parabolic-interpolated peak pick.
double estimate_fundamental_hz(const std::vector<float>& mono_signal,
                               int sample_rate, std::size_t fft_size);

struct SpectralPeak {
    double frequency_hz = 0.0;
    double magnitude    = 0.0;
};

// Find up to max_peaks local maxima in the magnitude spectrum, ranked by
// magnitude. Only peaks at least min_relative_height * global_max are kept.
// Returns peaks sorted ascending by frequency.
std::vector<SpectralPeak> find_top_peaks(const std::vector<float>& mono_signal,
                                         int sample_rate,
                                         std::size_t fft_size,
                                         std::size_t max_peaks,
                                         double min_relative_height = 0.1);

} // namespace bench
