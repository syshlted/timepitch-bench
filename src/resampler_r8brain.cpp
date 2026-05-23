#include "resampler.h"

// r8brain-free is header-only (C++11). No source files to compile.
#include <CDSPResampler.h>

#include <vector>

namespace bench {

namespace {

class R8BrainResampler final : public Resampler {
public:
    const char* name() const override { return "r8brain"; }

    std::size_t process(const ResampleConfig& cfg,
                        const float* in, std::size_t in_frames,
                        float* out, std::size_t max_out_frames) override {
        if (cfg.channels <= 0 || in_frames == 0) return 0;

        // r8brain works per-channel in doubles. Deinterleave, resample each
        // channel, then re-interleave. CDSPResampler24 is the recommended
        // quality tier (24-bit output, ~158 dB SNR) — analogous to libsr's
        // SRC_SINC_BEST_QUALITY.
        const int max_in_len = 1024; // r8brain's typical block size
        const int channels = cfg.channels;

        // Per-channel input as doubles.
        std::vector<std::vector<double>> in_ch(channels,
            std::vector<double>(in_frames));
        for (std::size_t f = 0; f < in_frames; ++f) {
            for (int c = 0; c < channels; ++c) {
                in_ch[c][f] = static_cast<double>(in[f * channels + c]);
            }
        }

        const std::size_t expected_out_frames =
            static_cast<std::size_t>(
                static_cast<double>(in_frames) * cfg.dst_rate / cfg.src_rate);
        const std::size_t cap = std::min(expected_out_frames, max_out_frames);

        std::vector<std::vector<double>> out_ch(channels,
            std::vector<double>(cap));

        // CDSPResampler24(SrcSampleRate, DstSampleRate, MaxInLen,
        //                 ReqTransBand = 2.0, ReqAtten = 158.0).
        // Defaults are the documented top-quality tier.
        for (int c = 0; c < channels; ++c) {
            r8b::CDSPResampler24 rs(cfg.src_rate, cfg.dst_rate, max_in_len);
            rs.oneshot<double, double>(in_ch[c].data(),
                                       static_cast<int>(in_frames),
                                       out_ch[c].data(),
                                       static_cast<int>(cap));
        }

        for (std::size_t f = 0; f < cap; ++f) {
            for (int c = 0; c < channels; ++c) {
                out[f * channels + c] = static_cast<float>(out_ch[c][f]);
            }
        }
        return cap;
    }
};

} // namespace

std::unique_ptr<Resampler> make_r8brain() {
    return std::make_unique<R8BrainResampler>();
}

} // namespace bench
