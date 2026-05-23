#include "resampler.h"

namespace bench {

std::vector<std::string> available_resamplers() {
    std::vector<std::string> v;
#ifdef BENCH_HAS_LIBSAMPLERATE
    v.emplace_back("libsamplerate");
#endif
#ifdef BENCH_HAS_R8BRAIN
    v.emplace_back("r8brain");
#endif
    return v;
}

std::unique_ptr<Resampler> make_resampler(const std::string& name) {
#ifdef BENCH_HAS_LIBSAMPLERATE
    if (name == "libsamplerate") return make_libsamplerate();
#endif
#ifdef BENCH_HAS_R8BRAIN
    if (name == "r8brain") return make_r8brain();
#endif
    (void)name;
    return nullptr;
}

} // namespace bench
