#include "stretcher.h"

#include <rubberband/RubberBandStretcher.h>

#include <vector>

namespace bench {

namespace {

using RB = RubberBand::RubberBandStretcher;

class RubberBandWrapper : public Stretcher {
public:
    const char* name() const override { return "rubberband"; }

    bool init(const StretchConfig& cfg) override {
        cfg_ = cfg;
        int opts = RB::OptionProcessRealTime
                 | RB::OptionEngineFiner
                 | RB::OptionThreadingNever;
        rb_ = std::make_unique<RB>(static_cast<size_t>(cfg.sample_rate),
                                   static_cast<size_t>(cfg.channels),
                                   opts,
                                   cfg.time_ratio,
                                   cfg.pitch_scale);
        rb_->setMaxProcessSize(static_cast<size_t>(cfg.block_size));
        return true;
    }

    void process(const float* in, std::size_t in_frames,
                 float* out, std::size_t max_out_frames,
                 std::size_t& out_frames) override {
        const int ch = cfg_.channels;

        in_planar_.assign(ch, std::vector<float>(in_frames, 0.0f));
        std::vector<const float*> in_ptrs(ch, nullptr);
        for (int c = 0; c < ch; ++c) {
            for (std::size_t i = 0; i < in_frames; ++i) {
                in_planar_[c][i] = in[i * ch + c];
            }
            in_ptrs[c] = in_planar_[c].data();
        }

        rb_->process(in_ptrs.data(), in_frames, false);

        std::size_t available =
            static_cast<std::size_t>(std::max(0, rb_->available()));
        std::size_t want = std::min(available, max_out_frames);

        out_planar_.assign(ch, std::vector<float>(want, 0.0f));
        std::vector<float*> out_ptrs(ch, nullptr);
        for (int c = 0; c < ch; ++c) out_ptrs[c] = out_planar_[c].data();

        std::size_t got = rb_->retrieve(out_ptrs.data(), want);
        for (std::size_t i = 0; i < got; ++i) {
            for (int c = 0; c < ch; ++c) {
                out[i * ch + c] = out_planar_[c][i];
            }
        }
        out_frames = got;
    }

    int latency_frames() const override {
        return rb_ ? static_cast<int>(rb_->getLatency()) : -1;
    }

private:
    StretchConfig cfg_{};
    std::unique_ptr<RB> rb_;
    std::vector<std::vector<float>> in_planar_, out_planar_;
};

} // namespace

std::unique_ptr<Stretcher> make_rubberband() {
    return std::make_unique<RubberBandWrapper>();
}

} // namespace bench
