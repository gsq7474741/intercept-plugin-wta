#pragma once
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace wta::types {

using Id         = int32_t;
using PlatformId = Id;
using TargetId   = Id;

struct Vec2 {
	float x{0}, y{0};
};
struct Vec3 {
	float x{0}, y{0}, z{0};
};

enum class PlatformRole : uint8_t
{
	AntiPersonnel,
	AntiArmor,
	MultiRole,
	Unknown
};
enum class TargetKind : uint8_t
{
	Infantry,
	Armor,
	SAM,
	Other
};

struct AmmoState {
	int missile{0};
	int bomb{0};
	int rocket{0};
};

// 弹夹详细信息
struct MagazineDetail {
	std::string name;        // 弹夹类名（如 "2Rnd_GBU12_LGB"）
	int ammo_count{0};       // 剩余弹药数
	bool loaded{false};      // 是否装载中
	int type{-1};            // 类型
	std::string location;    // 位置（如 "vest", "uniform", "backpack"）
};

struct PlatformState {
	PlatformId                   id{0};
	PlatformRole                 role{PlatformRole::Unknown};
	float                        hit_prob{0.f};
	float                        cost{0.f};
	Vec2                         pos{};
	float                        max_range{0.f};
	int                          max_targets{1};
	int                          quantity{1};
	bool                         alive{true};
	std::unordered_set<int>      target_types;
	AmmoState                    ammo{};
	std::string                  platform_type;  // 平台类型名称（如 "B_UAV_02_dynamicLoadout_F"）
	std::vector<MagazineDetail>  magazines;      // 弹夹详细信息列表
	float                        fuel{0.f};      // 剩余油量 (0.0-1.0)
	float                        damage{0.f};    // 总体损伤 (0.0-1.0, 0=无损伤, 1=完全损毁)
};

struct TargetState {
	TargetId         id{0};
	TargetKind       kind{TargetKind::Other};
	int              tier{0};
	float            value{0.f};
	Vec2             pos{};
	std::vector<int> prerequisites;
	bool             alive{true};
	std::string      target_type;              // 目标类型名称（如 "预警雷达站"）
	std::vector<int> prerequisite_targets;     // 前置目标ID列表（时序约束）
};

using AssignmentMatrix = std::vector<uint8_t>;

inline size_t idx_row_major(size_t i, size_t j, size_t n_targets)
{
	return i * n_targets + j;
}

}// namespace wta::types
