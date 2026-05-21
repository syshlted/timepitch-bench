#include "runner.h"
#include "fft.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>

namespace bench {

// Pull one channel out of an interleaved stereo buffer.
static std::vector<float> take_left(const std::vector<float>& interleaved,
                                    int channels) {
    std::vector<float> out;
    out.reserve(interleaved.size() / channels);
    for (std::size_t i = 0; i < interleaved.size(); i += channels) {
        out.push_back(interleaved[i]);
    }
    return out;
}

RunResult run_one(Stretcher& s, const RunOptions& opts) {
    RunResult r;
    r.stretcher_name = s.name();
    r.opts = opts;

    StretchConfig cfg;
    cfg.sample_rate = opts.sample_rate;
    cfg.channels = opts.channels;
    cfg.time_ratio = opts.time_ratio;
    cfg.pitch_scale = opts.pitch_scale;
    cfg.block_size = opts.block_size;

    if (!s.init(cfg)) {
        std::fprintf(stderr, "init failed for %s\n", s.name());
        return r;
    }

    const std::size_t total_in_frames =
        static_cast<std::size_t>(opts.duration_seconds * opts.sample_rate);

    auto input = generate_signal(opts.signal, opts.sample_rate, opts.channels,
                                 total_in_frames);

    // Output buffer sized generously for the worst-case stretch ratio plus
    // tail. The algorithm may return less.
    const std::size_t out_capacity =
        static_cast<std::size_t>(total_in_frames * std::max(2.0, opts.time_ratio * 2.0));
    std::vector<float> output(out_capacity * opts.channels, 0.0f);

    std::vector<float> out_block(opts.block_size * opts.channels * 4, 0.0f);

    std::size_t in_pos = 0, out_pos_frames = 0;
    while (in_pos < total_in_frames) {
        std::size_t block_frames =
            std::min<std::size_t>(opts.block_size, total_in_frames - in_pos);

        std::size_t produced = 0;
        auto t0 = std::chrono::steady_clock::now();
        s.process(input.data() + in_pos * opts.channels, block_frames,
                  out_block.data(),
                  out_block.size() / opts.channels,
                  produced);
        auto t1 = std::chrono::steady_clock::now();

        double wall_us =
            std::chrono::duration<double, std::micro>(t1 - t0).count();
        r.timing.record_call(wall_us);
        r.timing.total_wall_seconds += wall_us * 1e-6;
        r.timing.total_cpu_seconds  += wall_us * 1e-6;  // approx; same clock here
        r.timing.input_frames  += block_frames;
        r.timing.output_frames += produced;

        if (produced > 0) {
            std::size_t copy_frames =
                std::min(produced, out_capacity - out_pos_frames);
            std::memcpy(output.data() + out_pos_frames * opts.channels,
                        out_block.data(),
                        copy_frames * opts.channels * sizeof(float));
            out_pos_frames += copy_frames;
        }
        in_pos += block_frames;
    }

    output.resize(out_pos_frames * opts.channels);

    r.peak_rss_kb_after = peak_rss_kb();
    r.reported_latency_frames = s.latency_frames();

    // Quality scoring for tonal inputs: detect output fundamental.
    if (opts.signal == SignalKind::Sine) {
        const double input_hz = 1000.0;
        const double expected_hz = input_hz * opts.pitch_scale;
        auto mono = take_left(output, opts.channels);
        double detected = estimate_fundamental_hz(mono, opts.sample_rate,
                                                  opts.fft_size);
        r.quality.expected_hz = expected_hz;
        r.quality.detected_hz = detected;
        r.quality.pitch_error_cents = cents_between(expected_hz, detected);
    }

    return r;
}

void print_result(const RunResult& r) {
    std::printf("=== %s ===\n", r.stretcher_name.c_str());
    std::printf("  signal=%s time_ratio=%.3f pitch_scale=%.3f block=%d\n",
                signal_kind_name(r.opts.signal),
                r.opts.time_ratio, r.opts.pitch_scale, r.opts.block_size);
    std::printf("  input_frames=%zu output_frames=%zu calls=%zu\n",
                r.timing.input_frames, r.timing.output_frames, r.timing.calls);
    std::printf("  wall_total=%.3f ms  mean/call=%.2f us  p95/call=%.2f us\n",
                r.timing.total_wall_seconds * 1000.0,
                r.timing.mean_wall_us_per_call(),
                r.timing.p95_wall_us_per_call());
    std::printf("  realtime_factor=%.2fx  (>1 = faster than realtime)\n",
                r.timing.real_time_factor(r.opts.sample_rate));
    std::printf("  peak_rss=%zu KB\n", r.peak_rss_kb_after);
    if (r.opts.measure_latency) {
        std::printf("  reported_latency=%d frames\n", r.reported_latency_frames);
    }
    if (r.opts.signal == SignalKind::Sine) {
        std::printf("  expected=%.2f Hz  detected=%.2f Hz  error=%.2f cents\n",
                    r.quality.expected_hz, r.quality.detected_hz,
                    r.quality.pitch_error_cents);
    }
    std::printf("\n");
}

} // namespace bench
