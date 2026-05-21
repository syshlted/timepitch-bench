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

} // namespace bench
