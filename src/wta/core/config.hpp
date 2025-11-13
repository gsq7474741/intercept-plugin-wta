#pragma once
#include <optional>

namespace wta::config {

struct BPSOConfig {
	int                n_particles{500};
	int                n_iterations{50};
	float              w_max{0.8f};
	float              w_min{0.5f};
	float              c1{2.2f};
	float              c2{2.2f};
	float              v_max{1.8f};
	bool               use_gpu{false};
	std::optional<int> seed{};
};

struct ModelWeights {
	float value{0.7f};
	float cost{0.3f};
};

struct ModelConfig {
	bool         enable_tier_constraint{true};
	bool         enable_coverage_constraint{true};
	bool         enable_distance_constraint{true};
	ModelWeights weights{};
};

struct SolveConfig {
	BPSOConfig  bpso{};
	ModelConfig model{};
};

}// namespace wta::config
