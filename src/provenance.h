#pragma once

#include <string>
#include <vector>

namespace bench {

struct HostInfo {
    std::string hostname;
    std::string uname_sysname;
    std::string uname_release;
    std::string uname_version;
    std::string uname_machine;
    std::string cpu_model;
    std::vector<std::string> cpu_flags;
    int logical_cores = 0;
};

HostInfo gather_host_info();

// Returns current UTC time as ISO 8601 "YYYY-MM-DDTHH:MM:SSZ".
std::string utc_iso_timestamp();

// Same instant, but with ':' replaced by '-' so it's safe in filenames.
std::string utc_iso_timestamp_filename();

} // namespace bench
