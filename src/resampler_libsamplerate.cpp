#include "resampler.h"

#include <samplerate.h>

#include <cstring>

namespace bench {

namespace {

class LibsamplerateResampler final : public Resampler {
public:
    const char* name() const override { return "libsamplerate"; }

    std::size_t process(const ResampleConfig& cfg,
                        const float* in, std::size_t in_frames,
                        float* out, std::size_t max_out_frames) override {
        SRC_DATA data{};
        data.data_in = const_cast<float*>(in);
        data.data_out = out;
        data.input_frames = static_cast<long>(in_frames);
        data.output_frames = static_cast<long>(max_out_frames);
        data.src_ratio = static_cast<double>(cfg.dst_rate) / cfg.src_rate;
        data.end_of_input = 1;

        // SRC_SINC_BEST_QUALITY: the entire point — offline, highest tier.
        if (src_simple(&data, SRC_SINC_BEST_QUALITY, cfg.channels) != 0) {
            return 0;
        }
        return static_cast<std::size_t>(data.output_frames_gen);
    }
};

} // namespace

std::unique_ptr<Resampler> make_libsamplerate() {
    return std::make_unique<LibsamplerateResampler>();
}

} // namespace bench
