#pragma once

#include "metrics.h"
#include "signals.h"
#include "stretcher.h"

namespace bench {

struct RunOptions {
    SignalKind signal = SignalKind::Sine;
    int sample_rate = 48000;
    int channels = 2;
    double duration_seconds = 5.0;
    int block_size = 512;
    double time_ratio = 1.0;
    double pitch_scale = 1.0;
    bool measure_latency = false;
    std::size_t fft_size = 16384;
    double shepard_sweep_rate = 0.5; // octaves/sec; 0 = stationary
};

struct RunResult {
    std::string stretcher_name;
    RunOptions opts;
    CpuTiming timing;
    QualityReport quality;
    std::size_t peak_rss_kb_after = 0;
    int reported_latency_frames = -1;
};

RunResult run_one(Stretcher& s, const RunOptions& opts);

std::string format_result(const RunResult& r);
void print_result(const RunResult& r);

} // namespace bench
