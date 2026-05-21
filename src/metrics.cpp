#include "metrics.h"
#include "fft.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

#if defined(__linux__)
#include <sys/resource.h>
#endif

namespace bench {

void CpuTiming::record_call(double wall_us) {
    per_call_wall_us.push_back(wall_us);
    ++calls;
}

double CpuTiming::mean_wall_us_per_call() const {
    if (per_call_wall_us.empty()) return 0.0;
    double sum = 0.0;
    for (double v : per_call_wall_us) sum += v;
    return sum / static_cast<double>(per_call_wall_us.size());
}

double CpuTiming::p95_wall_us_per_call() const {
    if (per_call_wall_us.empty()) return 0.0;
    auto sorted = per_call_wall_us;
    std::sort(sorted.begin(), sorted.end());
    std::size_t idx = static_cast<std::size_t>(0.95 * (sorted.size() - 1));
    return sorted[idx];
}

double CpuTiming::real_time_factor(int sample_rate) const {
    if (total_cpu_seconds <= 0.0) return 0.0;
    double audio_seconds = static_cast<double>(output_frames) / sample_rate;
    return audio_seconds / total_cpu_seconds;
}

std::size_t peak_rss_kb() {
#if defined(__linux__)
    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) == 0) {
        // ru_maxrss is in kilobytes on Linux.
        return static_cast<std::size_t>(ru.ru_maxrss);
    }
#endif
    return 0;
}

double cents_between(double expected_hz, double detected_hz) {
    if (expected_hz <= 0.0 || detected_hz <= 0.0) return 0.0;
    return 1200.0 * std::log2(detected_hz / expected_hz);
}

double spectral_snr_db(const std::vector<float>& ref,
                       const std::vector<float>& cand,
                       std::size_t fft_size) {
    auto ref_mag  = magnitude_spectrum(ref, fft_size);
    auto cand_mag = magnitude_spectrum(cand, fft_size);
    std::size_t n = std::min(ref_mag.size(), cand_mag.size());
    if (n == 0) return -std::numeric_limits<double>::infinity();

    double ref_pow = 0.0, err_pow = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        ref_pow += ref_mag[i] * ref_mag[i];
        double d = ref_mag[i] - cand_mag[i];
        err_pow += d * d;
    }
    if (err_pow <= 0.0) return std::numeric_limits<double>::infinity();
    if (ref_pow <= 0.0) return -std::numeric_limits<double>::infinity();
    return 10.0 * std::log10(ref_pow / err_pow);
}

} // namespace bench
