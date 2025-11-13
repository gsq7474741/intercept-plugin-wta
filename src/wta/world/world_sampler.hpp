#pragma once
#include <memory>
#include "../core/solver_messages.hpp"

namespace wta::world {

struct IWorldSampler {
	virtual ~IWorldSampler()                              = default;
	virtual void sample(wta::proto::SolveRequest &io_req) = 0;
};

std::unique_ptr<IWorldSampler> make_intercept_world_sampler();

}// namespace wta::world
