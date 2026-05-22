#include "provenance.h"

#include <ctime>
#include <fstream>
#include <sstream>
#include <thread>

#include <sys/utsname.h>
#include <unistd.h>

namespace bench {

static std::string trim(const std::string& s) {
    auto b = s.find_first_not_of(" \t");
    auto e = s.find_last_not_of(" \t");
    return (b == std::string::npos) ? "" : s.substr(b, e - b + 1);
}

HostInfo gather_host_info() {
    HostInfo h;

    char hn[256] = {};
    if (gethostname(hn, sizeof(hn) - 1) == 0) h.hostname = hn;

    utsname u{};
    if (uname(&u) == 0) {
        h.uname_sysname = u.sysname;
        h.uname_release = u.release;
        h.uname_version = u.version;
        h.uname_machine = u.machine;
    }

    h.logical_cores = static_cast<int>(std::thread::hardware_concurrency());

    // Linux: parse /proc/cpuinfo for model name and flags. Best-effort —
    // missing fields stay empty on non-Linux.
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = trim(line.substr(0, colon));
        std::string val = trim(line.substr(colon + 1));
        if (h.cpu_model.empty() && (key == "model name" || key == "Model"))
            h.cpu_model = val;
        if (h.cpu_flags.empty() && (key == "flags" || key == "Features")) {
            std::istringstream is(val);
            std::string f;
            while (is >> f) h.cpu_flags.push_back(f);
        }
        if (!h.cpu_model.empty() && !h.cpu_flags.empty()) break;
    }

    return h;
}

std::string utc_iso_timestamp() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

std::string utc_iso_timestamp_filename() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H-%M-%SZ", &tm);
    return buf;
}

} // namespace bench
