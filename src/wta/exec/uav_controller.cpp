#include "uav_controller.hpp"
#include <intercept.hpp>
#include <cmath>
#include <algorithm>
#include <map>

namespace wta::exec {

using namespace intercept;

// ============================================================================
// 【仿照 fn_execution.sqf 重写】
// 核心原则：不使用 fireAtTarget/commandFire/doFire，
//          只设置好状态让 AI 自动开火，通过弹药消耗判断成功
// ============================================================================

// 武器类型枚举
enum class WeaponType {
    Missile,  // 导弹 - 高度 200m
    Rocket,   // 火箭 - 高度 100m
    Bomb,     // 炸弹 - 高度 150m
    Unknown   // 默认使用火箭高度
};

// 根据武器名称推断类型
static WeaponType get_weapon_type(const std::string& weapon_name) {
    std::string lower_name = weapon_name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
    
    if (lower_name.find("missile") != std::string::npos ||
        lower_name.find("scalpel") != std::string::npos ||
        lower_name.find("agm") != std::string::npos ||
        lower_name.find("dar") != std::string::npos) {
        return WeaponType::Missile;
    }
    
    if (lower_name.find("gbu") != std::string::npos ||
        lower_name.find("bomb") != std::string::npos) {
        return WeaponType::Bomb;
    }
    
    if (lower_name.find("rocket") != std::string::npos ||
        lower_name.find("skyfire") != std::string::npos) {
        return WeaponType::Rocket;
    }
    
    return WeaponType::Unknown;
}

// 根据武器类型获取攻击高度
// 【优化】降低高度以提高精度
static float get_attack_height(WeaponType type) {
    switch (type) {
        case WeaponType::Missile: return 150.0f;  // 导弹 150米（降低以便激光锁定）
        case WeaponType::Bomb:    return 100.0f;  // 炸弹 100米（更低以提高精度）
        case WeaponType::Rocket:  return 80.0f;   // 火箭 80米
        default:                  return 100.0f;  // 默认 100米
    }
}

bool UavController::navigate_to(UavEntity& uav, const wta::types::Vec2& target_pos, float altitude) {
    if (!uav.is_alive()) return false;
    
    try {
        client::invoker_lock lock;
        
        // 构造 3D 目标位置
        vector3 pos_3d(target_pos.x, target_pos.y, altitude);
        
        // 【仿照 fn_execution.sqf】设置 AI 战斗状态
        try {
            sqf::set_behaviour(uav.game_object(), "COMBAT");
        } catch (...) {}
        
        try {
            sqf::set_combat_mode(uav.game_object(), "RED");
        } catch (...) {}
        
        try {
            sqf::allow_fleeing(uav.game_object(), 0.0f);
        } catch (...) {}
        
        // 【仿照 fn_execution.sqf】同时使用 doMove 和 commandMove
        try {
            sqf::do_move(uav.game_object(), pos_3d);
        } catch (...) {}
        
        try {
            sqf::command_move(uav.game_object(), pos_3d);
        } catch (...) {}
        
        return true;
    } catch (...) {
        return false;
    }
}

bool UavController::aim_at(UavEntity& uav, const TargetEntity& target) {
    if (!uav.is_alive() || !target.is_alive()) return false;
    
    try {
        client::invoker_lock lock;
        
        // 【仿照 fn_execution.sqf】同时使用 doTarget 和 commandTarget
        try {
            sqf::reveal(uav.game_object(), target.game_object());
        } catch (...) {}
        
        try {
            sqf::do_target(uav.game_object(), target.game_object());
        } catch (...) {}
        
        try {
            sqf::command_target(uav.game_object(), target.game_object());
        } catch (...) {}
        
        return true;
    } catch (...) {
        return false;
    }
}

// ============================================================================
// 【核心】设置飞行高度（根据武器类型）
// 仿照 fn_execution.sqf 的 flyInHeight 逻辑
// ============================================================================
bool UavController::set_attack_height(UavEntity& uav, const std::string& weapon) {
    if (!uav.is_alive()) return false;
    
    try {
        client::invoker_lock lock;
        
        WeaponType wtype = get_weapon_type(weapon);
        float height = get_attack_height(wtype);
        
        sqf::fly_in_height(uav.game_object(), height);
        
        std::string type_name;
        switch (wtype) {
            case WeaponType::Missile: type_name = "Missile"; break;
            case WeaponType::Bomb:    type_name = "Bomb"; break;
            case WeaponType::Rocket:  type_name = "Rocket"; break;
            default:                  type_name = "Unknown"; break;
        }
        
        sqf::diag_log("[WTA][NAV] UAV " + std::to_string(uav.id()) + 
                     " flyInHeight set to " + std::to_string(static_cast<int>(height)) + 
                     "m (weapon type: " + type_name + ")");
        
        return true;
    } catch (...) {
        return false;
    }
}

// ============================================================================
// 【核心】记录弹药状态
// 仿照 fn_execution.sqf 的 magazinesAmmo 逻辑
// ============================================================================
int UavController::get_total_ammo(const UavEntity& uav) const {
    try {
        client::invoker_lock lock;
        
        auto mags = sqf::magazines_ammo(uav.game_object());
        int total = 0;
        
        for (const auto& mag : mags) {
            std::string lower_name = mag.name;
            std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
            
            // 【仿照 fn_execution.sqf】排除非消耗性物品
            if (lower_name.find("laserbatteries") == std::string::npos &&
                lower_name.find("battery") == std::string::npos &&
                lower_name.find("fuel") == std::string::npos &&
                lower_name.find("30rnd_65x39") == std::string::npos) {
                total += mag.count;
            }
        }
        
        return total;
    } catch (...) {
        return 0;
    }
}

// ============================================================================
// 【辅助函数】为目标创建激光标记
// ============================================================================
static std::map<wta::types::TargetId, intercept::types::object> g_laser_targets;

void UavController::create_laser_target(const TargetEntity& target) {
    try {
        client::invoker_lock lock;
        
        // 检查是否已有激光目标
        auto it = g_laser_targets.find(target.id());
        if (it != g_laser_targets.end() && !it->second.is_null()) {
            // 已存在且有效，跳过
            return;
        }
        
        // 获取目标位置
        auto target_pos = sqf::get_pos(target.game_object());
        
        // 创建激光目标对象
        auto laser_target = sqf::create_vehicle("LaserTargetW", target_pos);  // W = WEST 阵营
        
        if (!laser_target.is_null()) {
            // 将激光目标附加到目标上（跟随移动）
            sqf::attach_to(laser_target, target.game_object(), vector3(0, 0, 0));
            
            // 保存引用
            g_laser_targets[target.id()] = laser_target;
            
            sqf::diag_log("[WTA][LASER] Created LaserTarget for target " + std::to_string(target.id()));
        } else {
            sqf::diag_log("[WTA][LASER] Failed to create LaserTarget");
        }
    } catch (const std::exception& e) {
        sqf::diag_log("[WTA][LASER] EXCEPTION: " + std::string(e.what()));
    } catch (...) {
        sqf::diag_log("[WTA][LASER] UNKNOWN EXCEPTION");
    }
}

void UavController::remove_laser_target(const TargetEntity& target) {
    try {
        client::invoker_lock lock;
        
        auto it = g_laser_targets.find(target.id());
        if (it != g_laser_targets.end()) {
            if (!it->second.is_null()) {
                sqf::delete_vehicle(it->second);
            }
            g_laser_targets.erase(it);
            sqf::diag_log("[WTA][LASER] Removed LaserTarget for target " + std::to_string(target.id()));
        }
    } catch (...) {}
}

// ============================================================================
// 【完整 AI 机制】fire_at - 确保所有 AI 条件满足
// ============================================================================
bool UavController::fire_at(UavEntity& uav, const TargetEntity& target, const std::string& weapon) {
    try {
        client::invoker_lock lock;
        
        if (!uav.is_alive() || !target.is_alive()) {
            sqf::diag_log("[WTA][FIRE] Cannot fire: UAV or target not alive");
            return false;
        }
        
        sqf::diag_log("[WTA][FIRE] ===== FULL AI ATTACK SETUP =====");
        sqf::diag_log("[WTA][FIRE] UAV " + std::to_string(uav.id()) + " -> Target " + std::to_string(target.id()));
        
        // ========== 0. 【关键】为目标创建激光标记 ==========
        // 这样激光制导武器可以自动追踪目标，不依赖 AI 射击表现
        create_laser_target(target);
        
        // ========== 1. 获取 UAV 组 ==========
        auto uav_group = sqf::get_group(uav.game_object());
        if (uav_group.is_null()) {
            sqf::diag_log("[WTA][FIRE] ERROR: UAV has no group!");
            return false;
        }
        
        // ========== 2. 【关键】设置 UAV 自主模式 ==========
        try {
            sqf::set_autonomous(uav.game_object(), true);
            sqf::diag_log("[WTA][FIRE] setAutonomous = true");
        } catch (...) {
            sqf::diag_log("[WTA][FIRE] setAutonomous failed");
        }
        
        // ========== 3. 【关键】启用所有 AI 功能 ==========
        try {
            sqf::enable_ai(uav.game_object(), sqf::ai_behaviour_types::TARGET);
            sqf::enable_ai(uav.game_object(), sqf::ai_behaviour_types::AUTOTARGET);
            sqf::enable_ai(uav.game_object(), sqf::ai_behaviour_types::MOVE);
            sqf::enable_ai(uav.game_object(), sqf::ai_behaviour_types::FSM);
            sqf::diag_log("[WTA][FIRE] enableAI TARGET/AUTOTARGET/MOVE/FSM");
        } catch (...) {}
        
        // ========== 4. 【关键】启用组攻击 ==========
        try {
            sqf::enable_attack(uav_group, true);
            sqf::diag_log("[WTA][FIRE] enableAttack = true");
        } catch (...) {}
        
        // ========== 5. 设置战斗状态 ==========
        try {
            sqf::set_behaviour(uav_group, "COMBAT");
            sqf::set_combat_mode(uav_group, "RED");  // 自由开火
            sqf::allow_fleeing(uav_group, 0.0f);
            sqf::diag_log("[WTA][FIRE] behaviour=COMBAT, combatMode=RED");
        } catch (...) {}
        
        // ========== 6. 【关键】reveal 并验证 knowsAbout ==========
        try {
            // 多次 reveal 以提高知晓度
            sqf::reveal(uav_group, target.game_object());
            sqf::reveal(uav.game_object(), target.game_object());
            
            float knows = sqf::knows_about(uav_group, target.game_object());
            sqf::diag_log("[WTA][FIRE] knowsAbout = " + std::to_string(knows));
            
            // 如果知晓度不足，尝试直接设置目标
            if (knows < 4.0f) {
                sqf::diag_log("[WTA][FIRE] WARNING: knowsAbout < 4, trying additional reveal");
                // 再次 reveal
                sqf::reveal(uav_group, target.game_object());
            }
        } catch (...) {}
        
        // ========== 7. 【关键】获取炮手并设置 AI ==========
        try {
            auto gunner = sqf::gunner(uav.game_object());
            if (!gunner.is_null()) {
                sqf::enable_ai(gunner, sqf::ai_behaviour_types::TARGET);
                sqf::enable_ai(gunner, sqf::ai_behaviour_types::AUTOTARGET);
                sqf::reveal(gunner, target.game_object());
                sqf::do_target(gunner, target.game_object());
                sqf::diag_log("[WTA][FIRE] Gunner AI configured");
            } else {
                sqf::diag_log("[WTA][FIRE] No gunner found");
            }
        } catch (...) {
            sqf::diag_log("[WTA][FIRE] Gunner setup failed");
        }
        
        // ========== 8. 清除旧航点 ==========
        try {
            auto waypoints = sqf::waypoints(uav_group);
            for (int i = static_cast<int>(waypoints.size()) - 1; i >= 0; --i) {
                try { sqf::delete_waypoint(waypoints[i]); } catch (...) {}
            }
        } catch (...) {}
        
        // ========== 9. 创建 SAD 航点 ==========
        auto target_pos = sqf::get_pos(target.game_object());
        try {
            auto wp = sqf::add_waypoint(uav_group, target_pos, 100.0f);  // 扩大搜索半径
            sqf::set_waypoint_type(wp, sqf::waypoint::type::SAD);
            sqf::set_waypoint_behaviour(wp, sqf::waypoint::behaviour::COMBAT);
            sqf::set_waypoint_combat_mode(wp, sqf::waypoint::combat_mode::RED);
            sqf::set_waypoint_speed(wp, sqf::waypoint::speed::LIMITED);  // 降低速度，增加射击窗口
            sqf::set_current_waypoint(uav_group, wp);
            sqf::diag_log("[WTA][FIRE] SAD waypoint created (LIMITED speed)");
        } catch (...) {
            sqf::diag_log("[WTA][FIRE] Waypoint creation failed");
        }
        
        // ========== 9.1 设置低速飞行 ==========
        try {
            sqf::fly_in_height(uav.game_object(), 100.0f);  // 降低飞行高度
            sqf::diag_log("[WTA][FIRE] flyInHeight = 100m");
        } catch (...) {}
        
        // ========== 10. 设置目标并尝试开火命令 ==========
        try {
            sqf::do_target(uav.game_object(), target.game_object());
            sqf::command_target(uav.game_object(), target.game_object());
            sqf::diag_log("[WTA][FIRE] doTarget + commandTarget");
        } catch (...) {}
        
        // ========== 11. 【关键】直接发送开火命令 ==========
        try {
            // 使用 fireAtTarget（如果有武器）
            if (!weapon.empty()) {
                sqf::fire_at_target(uav.game_object(), target.game_object(), weapon);
                sqf::diag_log("[WTA][FIRE] fireAtTarget with weapon: " + weapon);
            } else {
                sqf::fire_at_target(uav.game_object(), target.game_object(), "");
                sqf::diag_log("[WTA][FIRE] fireAtTarget with default weapon");
            }
        } catch (...) {
            sqf::diag_log("[WTA][FIRE] fireAtTarget failed");
        }
        
        // ========== 12. 备用：简单开火命令 ==========
        try {
            sqf::send_simple_command(uav.game_object(), sqf::simple_command_type::FIRE);
            sqf::diag_log("[WTA][FIRE] send_simple_command FIRE");
        } catch (...) {}
        
        // ========== 13. 备用：doFire ==========
        try {
            sqf::do_fire(uav.game_object(), target.game_object());
            sqf::diag_log("[WTA][FIRE] doFire executed");
        } catch (...) {}
        
        sqf::diag_log("[WTA][FIRE] ===== AI ATTACK SETUP COMPLETE =====");
        return true;
        
    } catch (const std::exception& e) {
        sqf::diag_log("[WTA][FIRE] EXCEPTION: " + std::string(e.what()));
        return false;
    } catch (...) {
        sqf::diag_log("[WTA][FIRE] UNKNOWN EXCEPTION");
        return false;
    }
}

// ============================================================================
// 【新增】检查弹药是否消耗（用于验证开火是否成功）
// 仿照 fn_execution.sqf 的弹药检测逻辑
// ============================================================================
bool UavController::check_ammo_consumed(const UavEntity& uav, int ammo_before) const {
    int ammo_after = get_total_ammo(uav);
    int consumed = ammo_before - ammo_after;
    
    if (consumed > 0) {
        try {
            client::invoker_lock lock;
            sqf::diag_log("[WTA][FIRE] ✅ Ammo consumed: " + std::to_string(consumed) + 
                         " (before: " + std::to_string(ammo_before) + 
                         ", after: " + std::to_string(ammo_after) + ")");
        } catch (...) {}
        return true;
    }
    
    return false;
}

bool UavController::egress_to_safe(UavEntity& uav, const wta::types::Vec2* safe_pos) {
    if (!uav.is_alive()) return false;
    
    try {
        client::invoker_lock lock;
        
        wta::types::Vec2 egress_pos;
        if (safe_pos) {
            egress_pos = *safe_pos;
        } else {
            // 计算安全撤离位置（远离当前位置的反方向）
            auto current_pos = uav.position();
            egress_pos.x = current_pos.x - 2000.f;  // 向西撤离2km
            egress_pos.y = current_pos.y;
        }
        
        // 命令移动到安全位置
        vector3 safe_pos_3d(egress_pos.x, egress_pos.y, 500.f);  // 更高的高度
        sqf::do_move(uav.game_object(), safe_pos_3d);
        
        return true;
    } catch (...) {
        return false;
    }
}

bool UavController::return_to_base(UavEntity& uav) {
    if (!uav.is_alive()) return false;
    
    try {
        client::invoker_lock lock;
        
        // 命令 UAV 返回基地
        // 这里简化处理，实际应该从配置中获取基地位置
        auto current_pos = sqf::get_pos(uav.game_object());
        vector3 base_pos(current_pos.x - 5000.f, current_pos.y, 100.f);
        
        // 直接命令单位移动（不通过组）
        sqf::do_move(uav.game_object(), base_pos);
        
        return true;
    } catch (...) {
        return false;
    }
}

bool UavController::has_reached(const UavEntity& uav, const wta::types::Vec2& target_pos, float tolerance) const {
    auto current_pos = uav.position();
    float dist = distance(current_pos, target_pos);
    return dist <= tolerance;
}

bool UavController::is_in_range(const UavEntity& uav, const TargetEntity& target, float engagement_distance) const {
    auto uav_pos = uav.position();
    auto target_pos = target.position();
    float dist = distance(uav_pos, target_pos);
    return dist <= engagement_distance;
}

void UavController::stop(UavEntity& uav) {
    if (!uav.is_alive()) return;
    
    try {
        client::invoker_lock lock;
        sqf::do_stop(uav.game_object());
    } catch (...) {
        // Ignore
    }
}

float UavController::distance(const wta::types::Vec2& a, const wta::types::Vec2& b) const {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    return std::sqrt(dx * dx + dy * dy);
}

wta::types::Vec2 UavController::calculate_safe_egress_position(const UavEntity& uav, const wta::types::Vec2& threat_pos) const {
    auto uav_pos = uav.position();
    
    // 计算威胁方向的反方向
    float dx = uav_pos.x - threat_pos.x;
    float dy = uav_pos.y - threat_pos.y;
    float dist = std::sqrt(dx * dx + dy * dy);
    
    if (dist < 1.f) dist = 1.f;  // 避免除零
    
    // 归一化方向向量
    dx /= dist;
    dy /= dist;
    
    // 在反方向上移动 3km
    wta::types::Vec2 safe_pos;
    safe_pos.x = uav_pos.x + dx * 3000.f;
    safe_pos.y = uav_pos.y + dy * 3000.f;
    
    return safe_pos;
}

} // namespace wta::exec
