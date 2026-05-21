#include "stretcher.h"

// Signalsmith's fft.h calls std::memcpy without including <cstring>; under
// GCC 15's libstdc++ the transitive include is gone, so we pull it in first.
#include <cstring>

#include "signalsmith-stretch/signalsmith-stretch.h"

#include <vector>

namespace bench {

namespace {

class SignalsmithWrapper : public Stretcher {
public:
    const char* name() const override { return "signalsmith"; }

    bool init(const StretchConfig& cfg) override {
        cfg_ = cfg;
        stretch_.presetDefault(cfg.channels, static_cast<float>(cfg.sample_rate));
        stretch_.setTransposeFactor(static_cast<float>(cfg.pitch_scale));
        // Signalsmith stretches based on output length per call; we drive it
        // by asking for `block_size * time_ratio` output frames per input
        // block of `block_size` frames.
        return true;
    }

    void process(const float* in, std::size_t in_frames,
                 float* out, std::size_t max_out_frames,
                 std::size_t& out_frames) override {
        const int ch = cfg_.channels;

        // Deinterleave input.
        in_planar_.assign(ch, std::vector<float>(in_frames, 0.0f));
        in_ptrs_.assign(ch, nullptr);
        for (int c = 0; c < ch; ++c) {
            for (std::size_t i = 0; i < in_frames; ++i) {
                in_planar_[c][i] = in[i * ch + c];
            }
            in_ptrs_[c] = in_planar_[c].data();
        }

        std::size_t want_out =
            static_cast<std::size_t>(in_frames * cfg_.time_ratio);
        if (want_out > max_out_frames) want_out = max_out_frames;

        out_planar_.assign(ch, std::vector<float>(want_out, 0.0f));
        out_ptrs_.assign(ch, nullptr);
        for (int c = 0; c < ch; ++c) out_ptrs_[c] = out_planar_[c].data();

        stretch_.process(in_ptrs_.data(), static_cast<int>(in_frames),
                         out_ptrs_.data(), static_cast<int>(want_out));

        // Interleave.
        for (std::size_t i = 0; i < want_out; ++i) {
            for (int c = 0; c < ch; ++c) {
                out[i * ch + c] = out_planar_[c][i];
            }
        }
        out_frames = want_out;
    }

    int latency_frames() const override {
        return stretch_.inputLatency();
    }

private:
    StretchConfig cfg_{};
    signalsmith::stretch::SignalsmithStretch<float> stretch_;
    std::vector<std::vector<float>> in_planar_, out_planar_;
    std::vector<float*> in_ptrs_, out_ptrs_;
};

} // namespace

std::unique_ptr<Stretcher> make_signalsmith() {
    return std::make_unique<SignalsmithWrapper>();
}

} // namespace bench
