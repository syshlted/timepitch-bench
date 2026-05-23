#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace bench {

struct ResampleConfig {
    int src_rate = 44100;
    int dst_rate = 48000;
    int channels = 2;
};

// Offline resampler interface — parallel to `Stretcher` but with a one-shot
// API. Import is not realtime, so wrappers may buffer the whole input and
// emit the whole output in one call. Quality, not throughput, is the goal.
//
// Slated candidates (per D-016):
//   - libsamplerate (`SRC_SINC_BEST_QUALITY`, BSD-2) — first to land.
//   - r8brain-free (MIT) — second to compare against.
//   - SoX/soxr (LGPL) — skipped; LGPL is friction on iOS static-link.
class Resampler {
public:
    virtual ~Resampler() = default;

    virtual const char* name() const = 0;

    // Resamples `in` (interleaved float, `in_frames` per channel) to `out`
    // (interleaved float). Caller sizes `out` for the expected output frame
    // count (≈ in_frames * dst/src + slack). Returns the number of output
    // frames per channel written, or 0 on failure.
    virtual std::size_t process(const ResampleConfig& cfg,
                                const float* in, std::size_t in_frames,
                                float* out, std::size_t max_out_frames) = 0;
};

std::unique_ptr<Resampler> make_libsamplerate();
std::unique_ptr<Resampler> make_r8brain();

std::vector<std::string> available_resamplers();
std::unique_ptr<Resampler> make_resampler(const std::string& name);

} // namespace bench
