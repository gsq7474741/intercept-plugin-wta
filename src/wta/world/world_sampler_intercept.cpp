#include <intercept.hpp>
#include <string>
#include <cstring>
#include <chrono>
#include "../core/types.hpp"
#include "world_sampler.hpp"
#include "world_config.hpp"

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
	InterceptWorldSampler() {
		// 加载配置（只需加载一次）
		config_.load_from_sqf();
	}
	
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
				
				// 新增：平台类型名称
				plat.platform_type = veh_type;
				
				// 新增：油量和损伤
				plat.fuel = sqf::fuel(veh);
				plat.damage = sqf::damage(veh);
				
				// 新增：弹夹详细信息
				auto mags_full = sqf::magazines_ammo_full(veh);
				for (const auto& mag : mags_full) {
					wta::types::MagazineDetail mag_detail{};
					mag_detail.name = mag.name;
					mag_detail.ammo_count = mag.count;
					mag_detail.loaded = mag.loaded;
					mag_detail.type = mag.type;
					mag_detail.location = mag.location;
					plat.magazines.push_back(mag_detail);
				}
				
				// 从配置读取平台参数（如果有配置）
				auto platform_config = config_.get_platform_config(veh_type);
				if (platform_config.has_value()) {
					const auto& cfg = platform_config.value();
					plat.max_range = cfg.max_range_km * 1000.0f;  // km -> m
					plat.hit_prob = cfg.hit_prob;
					plat.cost = cfg.cost;
					plat.max_targets = cfg.max_targets;
					for (int tid : cfg.target_types) {
						plat.target_types.insert(tid);
					}
				} else {
					// 使用默认值
					plat.ammo = parse_ammo(veh);
					plat.max_range = estimate_range(plat.ammo);
					plat.hit_prob = 0.75f;
					plat.cost = 10.f;
					plat.max_targets = 1;
					// 可攻击所有目标类型
					plat.target_types.insert(static_cast<int>(wta::types::TargetKind::Infantry));
					plat.target_types.insert(static_cast<int>(wta::types::TargetKind::Armor));
					plat.target_types.insert(static_cast<int>(wta::types::TargetKind::SAM));
					plat.target_types.insert(static_cast<int>(wta::types::TargetKind::Other));
				}
				plat.quantity = 1;
				
				io_req.platforms.push_back(plat);
			}
			// 敌方载具 -> Target
			else if (veh_side != player_side && veh_side != sqf::civilian()) {
				// 检查是否有 wta_target_id 变量
				auto target_id_var = sqf::get_variable(veh, "wta_target_id");
				int assigned_target_id = target_id;
				try {
					if (!target_id_var.is_nil()) {
						float id_float = static_cast<float>(target_id_var);
						assigned_target_id = static_cast<int>(id_float);
					}
				} catch (...) {
					// 忽略转换错误，使用默认ID
				}
				
				wta::types::TargetState tgt{};
				tgt.id = assigned_target_id;
				tgt.kind = classify_target(veh_type.c_str());
				tgt.pos.x = static_cast<float>(veh_pos.x);
				tgt.pos.y = static_cast<float>(veh_pos.y);
				tgt.alive = true;
				
				// 从配置读取目标参数
				auto target_config = config_.get_target_config(assigned_target_id);
				if (target_config.has_value()) {
					const auto& cfg = target_config.value();
					tgt.target_type = cfg.type_name;
					tgt.value = cfg.value;
					tgt.tier = cfg.tier;
					for (int prereq : cfg.prerequisites) {
						tgt.prerequisite_targets.push_back(prereq);
					}
				} else {
					// 使用默认值
					tgt.target_type = veh_type;
					tgt.value = calc_target_value(tgt.kind);
					tgt.tier = 0;
				}
				
				io_req.targets.push_back(tgt);
				target_id++;
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
				// 检查是否有 wta_target_id 变量
				auto target_id_var = sqf::get_variable(unit, "wta_target_id");
				int assigned_target_id = target_id;
				try {
					if (!target_id_var.is_nil()) {
						float id_float = static_cast<float>(target_id_var);
						assigned_target_id = static_cast<int>(id_float);
					}
				} catch (...) {
					// 忽略转换错误，使用默认ID
				}
				
				auto unit_type = sqf::type_of(unit);
				auto unit_pos = sqf::get_pos(unit);
				
				wta::types::TargetState tgt{};
				tgt.id = assigned_target_id;
				tgt.kind = wta::types::TargetKind::Infantry;
				tgt.pos.x = static_cast<float>(unit_pos.x);
				tgt.pos.y = static_cast<float>(unit_pos.y);
				tgt.alive = true;
				
				// 从配置读取目标参数
				auto target_config = config_.get_target_config(assigned_target_id);
				if (target_config.has_value()) {
					const auto& cfg = target_config.value();
					tgt.target_type = cfg.type_name;
					tgt.value = cfg.value;
					tgt.tier = cfg.tier;
					for (int prereq : cfg.prerequisites) {
						tgt.prerequisite_targets.push_back(prereq);
					}
				} else {
					// 使用默认值
					tgt.target_type = unit_type;
					tgt.value = 20.f;
					tgt.tier = 0;
				}
				
				io_req.targets.push_back(tgt);
				target_id++;
			}
		}
		
		// 输出统计（每10秒输出一次，避免刷屏）
		static double last_log_time = 0.0;
		static int sample_count = 0;
		sample_count++;
		
		double current_time = std::chrono::duration<double>(
			std::chrono::steady_clock::now().time_since_epoch()
		).count();
		
		if (current_time - last_log_time >= 10.0) {
			char msg[256];
			sprintf_s(msg, sizeof(msg), 
				"WTA Sampler: %d platforms, %d targets (%d samples in 10s)", 
				(int)io_req.platforms.size(), (int)io_req.targets.size(), sample_count);
			sqf::diag_log(msg);
			last_log_time = current_time;
			sample_count = 0;
		}
	}
	
private:
	WorldConfig config_;
};

}// namespace

std::unique_ptr<IWorldSampler> make_intercept_world_sampler()
{
	return std::make_unique<InterceptWorldSampler>();
}

}// namespace wta::world
