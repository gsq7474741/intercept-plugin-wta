#include <intercept.hpp>
#include <string>
#include <cstring>
#include "../core/types.hpp"
#include "world_sampler.hpp"

namespace wta::world {

using namespace intercept;

namespace {

// 简单的字符串查找（忽略大小写）
bool str_contains_nocase(const char* haystack, const char* needle) {
	if (!haystack || !needle) return false;
	size_t hlen = strlen(haystack);
	size_t nlen = strlen(needle);
	if (nlen > hlen) return false;
	
	for (size_t i = 0; i <= hlen - nlen; ++i) {
		bool match = true;
		for (size_t j = 0; j < nlen; ++j) {
			char c1 = haystack[i + j];
			char c2 = needle[j];
			if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
			if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
			if (c1 != c2) {
				match = false;
				break;
			}
		}
		if (match) return true;
	}
	return false;
}

// 判断平台角色
wta::types::PlatformRole classify_platform(const char* type_name) {
	// AntiPersonnel: 炸弹型（Greyhawk, Sentinel）
	if (str_contains_nocase(type_name, "greyhawk") ||
	    str_contains_nocase(type_name, "sentinel") ||
	    str_contains_nocase(type_name, "uav_05")) {
		return wta::types::PlatformRole::AntiPersonnel;
	}
	
	// AntiArmor: 导弹型（Ababil, Tayran）
	if (str_contains_nocase(type_name, "ababil") ||
	    str_contains_nocase(type_name, "uav_02") ||
	    str_contains_nocase(type_name, "tayran")) {
		return wta::types::PlatformRole::AntiArmor;
	}
	
	return wta::types::PlatformRole::MultiRole;
}

// 判断目标类型
wta::types::TargetKind classify_target(const char* type_name) {
	// Infantry
	if (str_contains_nocase(type_name, "soldier") ||
	    str_contains_nocase(type_name, "infantry") ||
	    str_contains_nocase(type_name, "rifleman") ||
	    str_contains_nocase(type_name, "medic") ||
	    str_contains_nocase(type_name, "officer")) {
		return wta::types::TargetKind::Infantry;
	}
	
	// Armor
	if (str_contains_nocase(type_name, "tank") ||
	    str_contains_nocase(type_name, "apc") ||
	    str_contains_nocase(type_name, "ifv") ||
	    str_contains_nocase(type_name, "mrap")) {
		return wta::types::TargetKind::Armor;
	}
	
	// SAM
	if (str_contains_nocase(type_name, "aa") ||
	    str_contains_nocase(type_name, "sam") ||
	    str_contains_nocase(type_name, "radar")) {
		return wta::types::TargetKind::SAM;
	}
	
	return wta::types::TargetKind::Other;
}

// 计算目标价值
float calc_target_value(wta::types::TargetKind kind) {
	switch (kind) {
		case wta::types::TargetKind::Infantry: return 20.f;
		case wta::types::TargetKind::Armor: return 80.f;
		case wta::types::TargetKind::SAM: return 100.f;
		case wta::types::TargetKind::Other: return 30.f;
		default: return 10.f;
	}
}

// 解析弹药
wta::types::AmmoState parse_ammo(const object& unit) {
	wta::types::AmmoState ammo{};
	auto mags = sqf::magazines_ammo(unit);
	
	for (size_t i = 0; i < mags.size(); ++i) {
		const auto& mag = mags[i];
		const char* name = mag.name.c_str();
		int count = mag.count;
		
		// 导弹
		if (str_contains_nocase(name, "missile") ||
		    str_contains_nocase(name, "scalpel") ||
		    str_contains_nocase(name, "dagr")) {
			ammo.missile += count;
		}
		// 炸弹
		else if (str_contains_nocase(name, "bomb") ||
		         str_contains_nocase(name, "gbu") ||
		         str_contains_nocase(name, "lgb")) {
			ammo.bomb += count;
		}
		// 火箭
		else if (str_contains_nocase(name, "rocket") ||
		         str_contains_nocase(name, "hydra")) {
			ammo.rocket += count;
		}
	}
	
	return ammo;
}

// 估算射程
float estimate_range(const wta::types::AmmoState& ammo) {
	if (ammo.missile > 0) return 4000.f;
	if (ammo.bomb > 0) return 2000.f;
	if (ammo.rocket > 0) return 1500.f;
	return 500.f;
}

class InterceptWorldSampler final : public IWorldSampler {
public:
	void sample(wta::proto::SolveRequest &io_req) override
	{
		client::invoker_lock _lk;
		
		// 清空之前数据
		io_req.platforms.clear();
		io_req.targets.clear();
		
		// 获取玩家阵营
		auto player = sqf::player();
		auto player_side = sqf::get_side(player);
		
		int platform_id = 1;
		int target_id = 1;
		
		// ===== 采集载具（包括无人机） =====
		auto vehicles = sqf::vehicles();
		for (size_t i = 0; i < vehicles.size(); ++i) {
			const auto& veh = vehicles[i];
			if (!sqf::alive(veh)) continue;
			
			auto veh_side = sqf::get_side(veh);
			auto veh_type = sqf::type_of(veh);
			auto veh_pos = sqf::get_pos(veh);
			bool is_uav = sqf::unit_is_uav(veh);
			
			// 我方无人机 -> Platform
			if (veh_side == player_side && is_uav) {
				wta::types::PlatformState plat{};
				plat.id = platform_id++;
				plat.role = classify_platform(veh_type.c_str());
				plat.pos.x = static_cast<float>(veh_pos.x);
				plat.pos.y = static_cast<float>(veh_pos.y);
				plat.alive = true;
				
				// 弹药
				plat.ammo = parse_ammo(veh);
				plat.max_range = estimate_range(plat.ammo);
				plat.hit_prob = 0.75f;
				plat.cost = 10.f;
				plat.max_targets = 1;
				plat.quantity = 1;
				
				// 可攻击所有目标类型
				plat.target_types.insert(static_cast<int>(wta::types::TargetKind::Infantry));
				plat.target_types.insert(static_cast<int>(wta::types::TargetKind::Armor));
				plat.target_types.insert(static_cast<int>(wta::types::TargetKind::SAM));
				plat.target_types.insert(static_cast<int>(wta::types::TargetKind::Other));
				
				io_req.platforms.push_back(plat);
			}
			// 敌方载具 -> Target
			else if (veh_side != player_side && veh_side != sqf::civilian()) {
				wta::types::TargetState tgt{};
				tgt.id = target_id++;
				tgt.kind = classify_target(veh_type.c_str());
				tgt.pos.x = static_cast<float>(veh_pos.x);
				tgt.pos.y = static_cast<float>(veh_pos.y);
				tgt.alive = true;
				tgt.value = calc_target_value(tgt.kind);
				tgt.tier = 0;
				
				io_req.targets.push_back(tgt);
			}
		}
		
		// ===== 采集步兵 =====
		auto units = sqf::all_units();
		for (size_t i = 0; i < units.size(); ++i) {
			const auto& unit = units[i];
			if (!sqf::alive(unit)) continue;
			
			auto unit_side = sqf::get_side(unit);
			
			// 只采集敌方步兵作为目标
			if (unit_side != player_side && unit_side != sqf::civilian()) {
				auto unit_type = sqf::type_of(unit);
				auto unit_pos = sqf::get_pos(unit);
				
				wta::types::TargetState tgt{};
				tgt.id = target_id++;
				tgt.kind = wta::types::TargetKind::Infantry;
				tgt.pos.x = static_cast<float>(unit_pos.x);
				tgt.pos.y = static_cast<float>(unit_pos.y);
				tgt.alive = true;
				tgt.value = 20.f;
				tgt.tier = 0;
				
				io_req.targets.push_back(tgt);
			}
		}
		
		// 输出统计（使用diag_log避免依赖GLog）
		char msg[256];
		sprintf_s(msg, sizeof(msg), "WTA Sampler: %d platforms, %d targets", 
		          (int)io_req.platforms.size(), (int)io_req.targets.size());
		sqf::diag_log(msg);
	}
};

}// namespace

std::unique_ptr<IWorldSampler> make_intercept_world_sampler()
{
	return std::make_unique<InterceptWorldSampler>();
}

}// namespace wta::world
