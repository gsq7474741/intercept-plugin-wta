#include "uav_entity.hpp"
#include <intercept.hpp>
#include <cmath>

namespace wta::exec {

using namespace intercept;

// UavEntity 构造函数
UavEntity::UavEntity(wta::types::PlatformId id, const intercept::types::object& game_obj)
    : id_(id), game_obj_(std::make_shared<intercept::types::object>(game_obj)) {}

const intercept::types::object& UavEntity::game_object() const {
    return *game_obj_;
}

wta::types::Vec2 UavEntity::position() const {
    if (game_obj_ && !game_obj_->is_null()) {
        auto pos = sqf::get_pos(*game_obj_);
        return {pos.x, pos.y};
    }
    return {0.f, 0.f};
}

float UavEntity::altitude() const {
    if (game_obj_ && !game_obj_->is_null()) {
        auto pos = sqf::get_pos(*game_obj_);
        return pos.z;
    }
    return 0.f;
}

bool UavEntity::is_alive() const {
    if (!game_obj_ || game_obj_->is_null()) return false;
    return sqf::alive(*game_obj_);
}

bool UavEntity::has_ammo() const {
    if (!game_obj_ || game_obj_->is_null()) return false;
    auto mags = sqf::magazines(*game_obj_);
    return !mags.empty();
}

float UavEntity::fuel_level() const {
    if (!game_obj_ || game_obj_->is_null()) return 0.f;
    return sqf::fuel(*game_obj_);
}

float UavEntity::damage_level() const {
    if (!game_obj_ || game_obj_->is_null()) return 1.f;
    return sqf::damage(*game_obj_);
}

bool UavEntity::can_engage_target(const wta::types::Vec2& target_pos, float max_range) const {
    if (!is_alive() || !has_ammo()) return false;
    
    auto my_pos = position();
    float dx = target_pos.x - my_pos.x;
    float dy = target_pos.y - my_pos.y;
    float dist = std::sqrt(dx * dx + dy * dy);
    
    return dist <= max_range;
}

std::string UavEntity::get_best_weapon() const {
    if (!game_obj_ || game_obj_->is_null()) return "";
    
    auto weapons = sqf::weapons(*game_obj_);
    if (weapons.empty()) return "";
    
    // 优先使用导弹，然后炸弹，最后火箭弹
    for (const auto& weapon : weapons) {
        if (weapon.find("missile") != std::string::npos || 
            weapon.find("Missile") != std::string::npos ||
            weapon.find("AGM") != std::string::npos) {
            return weapon;
        }
    }
    
    for (const auto& weapon : weapons) {
        if (weapon.find("bomb") != std::string::npos || 
            weapon.find("Bomb") != std::string::npos ||
            weapon.find("GBU") != std::string::npos) {
            return weapon;
        }
    }
    
    for (const auto& weapon : weapons) {
        if (weapon.find("rocket") != std::string::npos || 
            weapon.find("Rocket") != std::string::npos ||
            weapon.find("HYDRA") != std::string::npos) {
            return weapon;
        }
    }
    
    // 返回第一个武器
    return weapons[0];
}

// TargetEntity 实现
TargetEntity::TargetEntity(wta::types::TargetId id, const intercept::types::object& game_obj)
    : id_(id), game_obj_(std::make_shared<intercept::types::object>(game_obj)) {}

const intercept::types::object& TargetEntity::game_object() const {
    return *game_obj_;
}

wta::types::Vec2 TargetEntity::position() const {
    if (game_obj_ && !game_obj_->is_null()) {
        auto pos = sqf::get_pos(*game_obj_);
        return {pos.x, pos.y};
    }
    return {0.f, 0.f};
}

bool TargetEntity::is_alive() const {
    if (!game_obj_ || game_obj_->is_null()) return false;
    return sqf::alive(*game_obj_);
}

} // namespace wta::exec
