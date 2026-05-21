#include "stretcher.h"

#include <SoundTouch.h>

#include <vector>

namespace bench {

namespace {

class SoundTouchWrapper : public Stretcher {
public:
    const char* name() const override { return "soundtouch"; }

    bool init(const StretchConfig& cfg) override {
        cfg_ = cfg;
        st_.setSampleRate(cfg.sample_rate);
        st_.setChannels(cfg.channels);
        // SoundTouch: tempo > 1 = faster (shorter), so it's the inverse of
        // our time_ratio convention. tempo = 1 / time_ratio.
        st_.setTempo(1.0 / cfg.time_ratio);
        st_.setPitch(cfg.pitch_scale);
        return true;
    }

    void process(const float* in, std::size_t in_frames,
                 float* out, std::size_t max_out_frames,
                 std::size_t& out_frames) override {
        st_.putSamples(in, static_cast<uint>(in_frames));
        uint received = st_.receiveSamples(out, static_cast<uint>(max_out_frames));
        out_frames = received;
    }

    int latency_frames() const override {
        // SoundTouch doesn't expose a clean latency API; ballpark via the
        // current internal buffer + tempo block size.
        return static_cast<int>(st_.numUnprocessedSamples());
    }

private:
    StretchConfig cfg_{};
    soundtouch::SoundTouch st_;
};

} // namespace

std::unique_ptr<Stretcher> make_soundtouch() {
    return std::make_unique<SoundTouchWrapper>();
}

} // namespace bench
