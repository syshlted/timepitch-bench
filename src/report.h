#pragma once

#include "provenance.h"
#include "runner.h"

#include <string>
#include <vector>

namespace bench {

// Build a filename describing the test parameters, prefixed with an
// ISO-timestamp safe for file paths.
//   2026-05-21T14-30-00Z_shepard_t1.000_p2.000_b512_sr48000_sweep0.500
std::string report_filename_stem(const std::string& iso_ts_filename,
                                 const RunOptions& opts);

// Write a JSON report file and a text mirror alongside it. The text mirror
// is the human-readable output from print_result() for each result.
// Returns the JSON path (or empty on failure).
std::string write_report(const std::string& out_dir,
                         const std::string& iso_ts,
                         const std::string& iso_ts_filename,
                         const HostInfo& host,
                         const RunOptions& opts,
                         const std::vector<RunResult>& results,
                         const std::string& text_mirror);

} // namespace bench
