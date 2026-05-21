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
        "  --library <name>        signalsmith | soundtouch | rubberband | all (default: all)\n"
        "  --signal  <name>        sine | sweep | impulse | noise (default: sine)\n"
        "  --time-ratio <f>        output/input duration (default: 1.0)\n"
        "  --pitch-scale <f>       output/input frequency (default: 1.0)\n"
        "  --duration <sec>        input duration in seconds (default: 5.0)\n"
        "  --block-size <n>        frames per process() call (default: 512)\n"
        "  --sample-rate <hz>      (default: 48000)\n"
        "  --measure-latency       report each library's reported algorithmic latency\n"
        "  --list                  list available libraries and exit\n",
        argv0);
}

int main(int argc, char** argv) {
    bench::RunOptions opts;
    std::string library = "all";

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

    for (const auto& name : targets) {
        auto s = bench::make_stretcher(name);
        if (!s) { std::fprintf(stderr, "unknown or disabled library: %s\n", name.c_str()); continue; }
        auto r = bench::run_one(*s, opts);
        bench::print_result(r);
    }
    return 0;
}
