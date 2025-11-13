#include <intercept.hpp>
#include <glog/logging.h>
#include "../core/types.hpp"
#include "world_sampler.hpp"

namespace wta::world {

using namespace intercept;

namespace {
class InterceptWorldSampler final : public IWorldSampler {
   public:
	void sample(wta::proto::SolveRequest &io_req) override
	{
		client::invoker_lock _lk;
		auto                 player = sqf::player();
		auto                 ppos   = sqf::get_pos(player);

		// 清空
		io_req.platforms.clear();
		io_req.targets.clear();
		
		// Demo Platform
		wta::types::PlatformState p{};
		p.id           = 1;
		p.role         = wta::types::PlatformRole::MultiRole;
		p.hit_prob     = 0.7f;
		p.cost         = 10.f;
		p.pos          = {static_cast<float>(ppos.x), static_cast<float>(ppos.y)};
		p.max_range    = 1500.f;
		p.max_targets  = 1;
		p.quantity     = 1;
		p.alive        = true;
		p.target_types = {1};
		io_req.platforms.push_back(std::move(p));

		// Demo Target
		wta::types::TargetState t{};
		t.id    = 1;
		t.kind  = wta::types::TargetKind::Infantry;
		t.tier  = 0;
		t.value = 20.f;
		t.pos   = {static_cast<float>(ppos.x + 50.f), static_cast<float>(ppos.y + 50.f)};
		t.alive = true;
		io_req.targets.push_back(std::move(t));
		
		// LOG(INFO) << "Demo sampling: 1 platform, 1 target";
	}
};
}// namespace

std::unique_ptr<IWorldSampler> make_intercept_world_sampler()
{
	return std::make_unique<InterceptWorldSampler>();
}

}// namespace wta::world
