#include "report.h"
#include "build_info.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

namespace bench {

static std::string json_escape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;
            case '\r': o += "\\r";  break;
            case '\t': o += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    o += buf;
                } else {
                    o += c;
                }
        }
    }
    return o;
}

static std::string fmt_num(double x) {
    if (std::isnan(x) || std::isinf(x)) return "null";
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.6g", x);
    return buf;
}

static bool mkdir_p(const std::string& path) {
    if (path.empty()) return true;
    std::string acc;
    for (std::size_t i = 0; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == '/') {
            if (!acc.empty() && acc != ".") {
                if (mkdir(acc.c_str(), 0755) != 0) {
                    // ignore EEXIST
                }
            }
        }
        if (i < path.size()) acc += path[i];
    }
    return true;
}

std::string report_filename_stem(const std::string& ts, const RunOptions& opts) {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "%s_%s_t%.3f_p%.3f_b%d_sr%d_sweep%.3f",
        ts.c_str(),
        signal_kind_name(opts.signal),
        opts.time_ratio,
        opts.pitch_scale,
        opts.block_size,
        opts.sample_rate,
        opts.shepard_sweep_rate);
    return buf;
}

static void emit_string_array(std::ostringstream& o,
                              const std::vector<std::string>& v) {
    o << '[';
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i) o << ',';
        o << '"' << json_escape(v[i]) << '"';
    }
    o << ']';
}

static void emit_double_array(std::ostringstream& o,
                              const std::vector<double>& v) {
    o << '[';
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i) o << ',';
        o << fmt_num(v[i]);
    }
    o << ']';
}

static void emit_one_result(std::ostringstream& o, const RunResult& r) {
    o << "{\n";
    o << "  \"library\": \"" << json_escape(r.stretcher_name) << "\",\n";
    o << "  \"timing\": {\n";
    o << "    \"wall_total_ms\": "    << fmt_num(r.timing.total_wall_seconds * 1000.0) << ",\n";
    o << "    \"mean_us_per_call\": " << fmt_num(r.timing.mean_wall_us_per_call())     << ",\n";
    o << "    \"p95_us_per_call\": "  << fmt_num(r.timing.p95_wall_us_per_call())      << ",\n";
    o << "    \"realtime_factor\": "  << fmt_num(r.timing.real_time_factor(r.opts.sample_rate)) << ",\n";
    o << "    \"calls\": "            << r.timing.calls            << ",\n";
    o << "    \"input_frames\": "     << r.timing.input_frames     << ",\n";
    o << "    \"output_frames\": "    << r.timing.output_frames    << "\n";
    o << "  },\n";
    o << "  \"peak_rss_kb\": " << r.peak_rss_kb_after << ",\n";
    if (r.opts.measure_latency) {
        o << "  \"reported_latency_frames\": " << r.reported_latency_frames << ",\n";
    }
    o << "  \"quality\": {\n";
    if (r.opts.signal == SignalKind::Sine) {
        o << "    \"sine\": {\n";
        o << "      \"expected_hz\": "        << fmt_num(r.quality.expected_hz)        << ",\n";
        o << "      \"detected_hz\": "        << fmt_num(r.quality.detected_hz)        << ",\n";
        o << "      \"pitch_error_cents\": "  << fmt_num(r.quality.pitch_error_cents)  << "\n";
        o << "    }\n";
    } else if (r.opts.signal == SignalKind::Shepard) {
        o << "    \"shepard\": {\n";
        o << "      \"input_peaks_hz\": ";
        emit_double_array(o, r.quality.shepard_input_peaks_hz);
        o << ",\n";
        o << "      \"output_peaks_hz\": ";
        emit_double_array(o, r.quality.shepard_output_peaks_hz);
        o << ",\n";
        o << "      \"median_octave_ratio\": "   << fmt_num(r.quality.shepard_median_octave_ratio)   << ",\n";
        o << "      \"observed_pitch_ratio\": "  << fmt_num(r.quality.shepard_observed_pitch_ratio)  << ",\n";
        o << "      \"envelope\": {\n";
        o << "        \"center_hz\": "       << fmt_num(r.quality.shepard_envelope_center_hz)       << ",\n";
        o << "        \"sigma_oct\": "       << fmt_num(r.quality.shepard_envelope_sigma_oct)       << ",\n";
        o << "        \"input_center_hz\": " << fmt_num(r.quality.shepard_input_envelope_center_hz) << ",\n";
        o << "        \"input_sigma_oct\": " << fmt_num(r.quality.shepard_input_envelope_sigma_oct) << ",\n";
        o << "        \"fit_ok\": "          << (r.quality.shepard_envelope_fit_ok       ? "true" : "false") << ",\n";
        o << "        \"input_fit_ok\": "    << (r.quality.shepard_input_envelope_fit_ok ? "true" : "false") << "\n";
        o << "      }\n";
        o << "    }\n";
    } else {
        o << "    \"none\": true\n";
    }
    o << "  }\n";
    o << "}";
}

std::string write_report(const std::string& out_dir,
                         const std::string& iso_ts,
                         const std::string& iso_ts_filename,
                         const HostInfo& host,
                         const RunOptions& opts,
                         const std::vector<RunResult>& results,
                         const std::string& text_mirror) {
    if (!mkdir_p(out_dir)) return {};

    std::string stem = report_filename_stem(iso_ts_filename, opts);
    std::string json_path = out_dir + "/" + stem + ".json";
    std::string txt_path  = out_dir + "/" + stem + ".txt";

    std::ostringstream o;
    o.setf(std::ios::fmtflags(0));

    o << "{\n";
    o << "\"schema_version\": 1,\n";
    o << "\"timestamp_utc\": \"" << iso_ts << "\",\n";

    o << "\"params\": {\n";
    o << "  \"signal\": \"" << signal_kind_name(opts.signal) << "\",\n";
    o << "  \"time_ratio\": "        << fmt_num(opts.time_ratio)         << ",\n";
    o << "  \"pitch_scale\": "       << fmt_num(opts.pitch_scale)        << ",\n";
    o << "  \"block_size\": "        << opts.block_size                  << ",\n";
    o << "  \"sample_rate\": "       << opts.sample_rate                 << ",\n";
    o << "  \"channels\": "          << opts.channels                    << ",\n";
    o << "  \"duration_seconds\": "  << fmt_num(opts.duration_seconds)   << ",\n";
    o << "  \"fft_size\": "          << opts.fft_size                    << ",\n";
    o << "  \"shepard_sweep_rate\": " << fmt_num(opts.shepard_sweep_rate) << ",\n";
    o << "  \"measure_latency\": "   << (opts.measure_latency ? "true" : "false") << "\n";
    o << "},\n";

    o << "\"build\": {\n";
    o << "  \"type\": \""             << json_escape(build_info::build_type)        << "\",\n";
    o << "  \"compiler_id\": \""      << json_escape(build_info::compiler_id)       << "\",\n";
    o << "  \"compiler_version\": \"" << json_escape(build_info::compiler_version)  << "\",\n";
    o << "  \"compiler_flags\": \""   << json_escape(build_info::compiler_flags)    << "\",\n";
    o << "  \"build_timestamp\": \""  << json_escape(build_info::build_timestamp)   << "\",\n";
    o << "  \"dsp_bench_git_rev\": \""<< json_escape(build_info::dsp_bench_git_rev) << "\",\n";
    o << "  \"dsp_bench_git_dirty\": "<< build_info::dsp_bench_git_dirty            << "\n";
    o << "},\n";

    o << "\"libraries\": {\n";
    o << "  \"signalsmith\": {\"git_rev\": \"" << build_info::signalsmith_git_rev
      << "\", \"tag\": \"" << build_info::signalsmith_tag << "\"},\n";
    o << "  \"soundtouch\":  {\"git_rev\": \"" << build_info::soundtouch_git_rev
      << "\", \"tag\": \"" << build_info::soundtouch_tag  << "\"},\n";
    o << "  \"rubberband\":  {\"git_rev\": \"" << build_info::rubberband_git_rev
      << "\", \"tag\": \"" << build_info::rubberband_tag  << "\"}\n";
    o << "},\n";

    o << "\"host\": {\n";
    o << "  \"hostname\": \""        << json_escape(host.hostname)      << "\",\n";
    o << "  \"uname_sysname\": \""   << json_escape(host.uname_sysname) << "\",\n";
    o << "  \"uname_release\": \""   << json_escape(host.uname_release) << "\",\n";
    o << "  \"uname_version\": \""   << json_escape(host.uname_version) << "\",\n";
    o << "  \"uname_machine\": \""   << json_escape(host.uname_machine) << "\",\n";
    o << "  \"cpu_model\": \""       << json_escape(host.cpu_model)     << "\",\n";
    o << "  \"logical_cores\": "     << host.logical_cores              << ",\n";
    o << "  \"cpu_flags\": ";
    emit_string_array(o, host.cpu_flags);
    o << "\n},\n";

    o << "\"results\": [\n";
    for (std::size_t i = 0; i < results.size(); ++i) {
        if (i) o << ",\n";
        emit_one_result(o, results[i]);
    }
    o << "\n]\n";
    o << "}\n";

    std::ofstream jf(json_path);
    if (!jf) return {};
    jf << o.str();
    jf.close();

    std::ofstream tf(txt_path);
    if (tf) tf << text_mirror;

    return json_path;
}

} // namespace bench
