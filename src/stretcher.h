#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace bench {

struct StretchConfig {
    int sample_rate = 48000;
    int channels = 2;
    double time_ratio = 1.0;   // 1.0 = no stretch; >1 longer; <1 shorter
    double pitch_scale = 1.0;  // 1.0 = no shift; 2.0 = +1 octave
    int block_size = 512;      // frames per process() call
};

// Common interface over the three candidate libraries. Real-time oriented:
// process() is called repeatedly with fixed-size input blocks and pulls
// whatever output the algorithm has ready into `out`. `out_frames` reports
// how many output frames were produced (may be 0 while the algorithm primes).
class Stretcher {
public:
    virtual ~Stretcher() = default;

    virtual const char* name() const = 0;

    // Returns false on construction failure.
    virtual bool init(const StretchConfig& cfg) = 0;

    // Interleaved float input/output. `in_frames` frames in, up to
    // `max_out_frames` frames written to `out`. Sets `out_frames`.
    virtual void process(const float* in, std::size_t in_frames,
                         float* out, std::size_t max_out_frames,
                         std::size_t& out_frames) = 0;

    // Algorithmic latency in input frames, if reported by the library.
    // Returns -1 if unknown.
    virtual int latency_frames() const { return -1; }
};

std::unique_ptr<Stretcher> make_signalsmith();
std::unique_ptr<Stretcher> make_soundtouch();
std::unique_ptr<Stretcher> make_rubberband();

std::vector<std::string> available_stretchers();
std::unique_ptr<Stretcher> make_stretcher(const std::string& name);

} // namespace bench
