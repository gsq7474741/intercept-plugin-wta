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

struct PlatformState {
	PlatformId              id{0};
	PlatformRole            role{PlatformRole::Unknown};
	float                   hit_prob{0.f};
	float                   cost{0.f};
	Vec2                    pos{};
	float                   max_range{0.f};
	int                     max_targets{1};
	int                     quantity{1};
	bool                    alive{true};
	std::unordered_set<int> target_types;
	AmmoState               ammo{};
};

struct TargetState {
	TargetId         id{0};
	TargetKind       kind{TargetKind::Other};
	int              tier{0};
	float            value{0.f};
	Vec2             pos{};
	std::vector<int> prerequisites;
	bool             alive{true};
};

using AssignmentMatrix = std::vector<uint8_t>;

inline size_t idx_row_major(size_t i, size_t j, size_t n_targets)
{
	return i * n_targets + j;
}

}// namespace wta::types
