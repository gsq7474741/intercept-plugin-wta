#pragma once
#include <string>
#include <vector>
#include "types.hpp"
#include "config.hpp"

namespace wta::proto {

struct SolveRequest {
    std::string mission_id;
    double timestamp{0.0};
    wta::config::SolveConfig config{};
    std::vector<wta::types::PlatformState> platforms;
    std::vector<wta::types::TargetState>   targets;
};

struct AssignmentDetails {
    bool is_valid{false};
    double coverage_rate{0.0};
};

struct SolveStats {
    double computation_time{0.0};
    int iterations{0};
};

struct SolveResponse {
    std::string status;
    double best_fitness{0.0};
    wta::types::AssignmentMatrix assignment;
    size_t n_platforms{0};
    size_t n_targets{0};
    AssignmentDetails details{};
    SolveStats stats{};
    double ttl_sec{2.0};
};

} // namespace wta::proto
