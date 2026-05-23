#include "provenance.h"
#include "report.h"
#include "resample_runner.h"
#include "resampler.h"
#include "runner.h"
#include "stretcher.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace bench {

std::vector<std::string> available_stretchers() {
    std::vector<std::string> v;
#if BENCH_HAS_SIGNALSMITH
    v.emplace_back("signalsmith");
#endif
#if BENCH_HAS_SOUNDTOUCH
    v.emplace_back("soundtouch");
#endif
#if BENCH_HAS_RUBBERBAND
    v.emplace_back("rubberband");
#endif
    return v;
}

std::unique_ptr<Stretcher> make_stretcher(const std::string& name) {
#if BENCH_HAS_SIGNALSMITH
    if (name == "signalsmith") return make_signalsmith();
#endif
#if BENCH_HAS_SOUNDTOUCH
    if (name == "soundtouch")  return make_soundtouch();
#endif
#if BENCH_HAS_RUBBERBAND
    if (name == "rubberband")  return make_rubberband();
#endif
    return nullptr;
}

} // namespace bench

static void usage(const char* argv0) {
    std::fprintf(stderr,
        "usage: %s [options]\n"
        "Stretcher mode (default):\n"
        "  --library <name>        signalsmith | soundtouch | rubberband | all (default: all)\n"
        "  --signal  <name>        sine | sweep | impulse | noise | shepard (default: sine)\n"
        "  --time-ratio <f>        output/input duration (default: 1.0)\n"
        "  --pitch-scale <f>       output/input frequency (default: 1.0)\n"
        "  --duration <sec>        input duration in seconds (default: 5.0)\n"
        "  --block-size <n>        frames per process() call (default: 512)\n"
        "  --sample-rate <hz>      (default: 48000)\n"
        "  --measure-latency       report each library's reported algorithmic latency\n"
        "  --shepard-sweep-rate <oct/sec>  (default: 0.5; 0 = stationary)\n"
        "  --out-dir <path>        directory for report files (default: ./reports)\n"
        "  --no-save               do not write report files, stdout only\n"
        "  --list                  list available stretchers and exit\n"
        "\n"
        "Resampler mode (--resample):\n"
        "  --resample              switch to import-path resampler measurement mode\n"
        "  --resample-library <n>  libsamplerate | r8brain | all (default: all)\n"
        "  --resample-signal <n>   sine | alias | impulse | sweep | all (default: all)\n"
        "  --src-rate <hz>         source sample rate (default: 44100)\n"
        "  --dst-rate <hz>         destination sample rate (default: 48000)\n"
        "  --duration <sec>        input duration in seconds (default: 2.0)\n"
        "  --sine-hz <hz>          sine frequency for sine test (default: 1000)\n"
        "  --list-resamplers       list available resamplers and exit\n",
        argv0);
}

namespace {

int run_resample_mode(int argc, char** argv) {
    bench::ResampleRunOptions opts;
    opts.duration_seconds = 2.0;
    std::string library = "all";
    std::string signal  = "all";

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) { usage(argv[0]); std::exit(2); }
            return argv[++i];
        };
        if (a == "--resample")                {}
        else if (a == "--resample-library")   library = next();
        else if (a == "--resample-signal")    signal  = next();
        else if (a == "--src-rate")           opts.src_rate = std::stoi(next());
        else if (a == "--dst-rate")           opts.dst_rate = std::stoi(next());
        else if (a == "--duration")           opts.duration_seconds = std::stod(next());
        else if (a == "--sine-hz")            opts.sine_hz = std::stod(next());
        else if (a == "--list-resamplers") {
            for (auto& n : bench::available_resamplers()) std::printf("%s\n", n.c_str());
            return 0;
        }
        else if (a == "-h" || a == "--help") { usage(argv[0]); return 0; }
        else { usage(argv[0]); return 2; }
    }

    std::vector<std::string> libs;
    if (library == "all") libs = bench::available_resamplers();
    else libs.push_back(library);

    std::vector<bench::ResampleSignal> signals;
    if (signal == "all") {
        // Sweep is omitted from default-all: spectral_snr_db on a sweep is
        // dominated by group-delay alignment and gives a measurement-floor
        // number that doesn't distinguish libraries. Run --resample-signal
        // sweep explicitly if you want the raw output for offline plotting.
        signals = {bench::ResampleSignal::Sine,
                   bench::ResampleSignal::SineNearSrcNy,
                   bench::ResampleSignal::Impulse};
    } else {
        signals.push_back(bench::parse_resample_signal(signal));
    }

    for (const auto& name : libs) {
        auto r = bench::make_resampler(name);
        if (!r) {
            std::fprintf(stderr, "unknown or disabled resampler: %s\n", name.c_str());
            continue;
        }
        for (auto sig : signals) {
            auto sub_opts = opts;
            sub_opts.signal = sig;
            auto res = bench::run_one_resample(*r, sub_opts);
            bench::print_resample_result(res);
        }
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    // Resampler mode is a separate sub-command, triggered by --resample anywhere.
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--resample") return run_resample_mode(argc, argv);
    }

    bench::RunOptions opts;
    std::string library = "all";
    std::string out_dir = "reports";
    bool save = true;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) { usage(argv[0]); std::exit(2); }
            return argv[++i];
        };
        if (a == "--library")            library = next();
        else if (a == "--signal")        opts.signal = bench::parse_signal_kind(next());
        else if (a == "--time-ratio")    opts.time_ratio = std::stod(next());
        else if (a == "--pitch-scale")   opts.pitch_scale = std::stod(next());
        else if (a == "--duration")      opts.duration_seconds = std::stod(next());
        else if (a == "--block-size")    opts.block_size = std::stoi(next());
        else if (a == "--sample-rate")   opts.sample_rate = std::stoi(next());
        else if (a == "--measure-latency") opts.measure_latency = true;
        else if (a == "--shepard-sweep-rate") opts.shepard_sweep_rate = std::stod(next());
        else if (a == "--out-dir")       out_dir = next();
        else if (a == "--no-save")       save = false;
        else if (a == "--list") {
            for (auto& n : bench::available_stretchers()) std::printf("%s\n", n.c_str());
            return 0;
        }
        else if (a == "-h" || a == "--help") { usage(argv[0]); return 0; }
        else { usage(argv[0]); return 2; }
    }

    std::vector<std::string> targets;
    if (library == "all") targets = bench::available_stretchers();
    else targets.push_back(library);

    std::vector<bench::RunResult> results;
    std::string text_mirror;

    for (const auto& name : targets) {
        auto s = bench::make_stretcher(name);
        if (!s) { std::fprintf(stderr, "unknown or disabled library: %s\n", name.c_str()); continue; }
        auto r = bench::run_one(*s, opts);
        std::string formatted = bench::format_result(r);
        std::fputs(formatted.c_str(), stdout);
        text_mirror += formatted;
        results.push_back(std::move(r));
    }

    if (save && !results.empty()) {
        auto host = bench::gather_host_info();
        std::string ts        = bench::utc_iso_timestamp();
        std::string ts_file   = bench::utc_iso_timestamp_filename();
        std::string path = bench::write_report(out_dir, ts, ts_file, host,
                                               opts, results, text_mirror);
        if (path.empty()) {
            std::fprintf(stderr, "warning: failed to write report to %s\n",
                         out_dir.c_str());
        } else {
            std::fprintf(stderr, "report: %s\n", path.c_str());
        }
    }
    return 0;
}
