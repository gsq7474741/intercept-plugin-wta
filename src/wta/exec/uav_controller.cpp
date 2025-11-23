#include "uav_controller.hpp"
#include <intercept.hpp>
#include <cmath>

namespace wta::exec {

using namespace intercept;

bool UavController::navigate_to(UavEntity& uav, const wta::types::Vec2& target_pos, float altitude) {
    if (!uav.is_alive()) return false;
    
    try {
        client::invoker_lock lock;
        
        // 构造 3D 目标位置
        vector3 pos_3d(target_pos.x, target_pos.y, altitude);
        
        try {
            // 【关键修复1】设置AI行为模式为战斗状态
            sqf::set_behaviour(uav.game_object(), "COMBAT");
        } catch (...) {
            // 忽略设置失败，继续执行
        }
        
        try {
            // 【关键修复2】设置交战规则为自由开火（RED = 主动攻击）
            sqf::set_combat_mode(uav.game_object(), "RED");
        } catch (...) {
            // 忽略设置失败，继续执行
        }
        
        try {
            // 【关键修复3】禁止AI逃跑行为（Arma 3 AI在受压时会撤退）
            sqf::allow_fleeing(uav.game_object(), 0.0f);
        } catch (...) {
            // 忽略设置失败
        }
        
        try {
            // 获取 UAV 的 AI 驾驶员并命令移动
            auto pilot = sqf::driver(uav.game_object());
            if (!pilot.is_null()) {
                sqf::do_move(pilot, pos_3d);
            } else {
                // 如果没有驾驶员，直接命令载具
                sqf::do_move(uav.game_object(), pos_3d);
            }
        } catch (...) {
            // 移动命令失败，尝试直接命令载具
            try {
                sqf::do_move(uav.game_object(), pos_3d);
            } catch (...) {
                return false;  // 移动失败，返回失败
            }
        }
        
        try {
            // 确保弹药满
            sqf::set_vehicle_ammo_def(uav.game_object(), 1.0f);
        } catch (...) {
            // 忽略设置失败
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

bool UavController::aim_at(UavEntity& uav, const TargetEntity& target) {
    if (!uav.is_alive() || !target.is_alive()) return false;
    
    try {
        client::invoker_lock lock;
        
        try {
            // 【关键修复3】让AI发现目标（提高knowsAbout值到4 = 完全知晓）
            sqf::reveal(uav.game_object(), target.game_object());
        } catch (...) {
            // reveal失败不致命，继续
        }
        
        try {
            // 命令 UAV 瞄准目标
            sqf::do_target(uav.game_object(), target.game_object());
        } catch (...) {
            return false;  // 瞄准失败是致命的
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

bool UavController::fire_at(UavEntity& uav, const TargetEntity& target, const std::string& weapon) {
    if (!uav.is_alive() || !target.is_alive()) return false;
    if (!uav.has_ammo()) return false;
    
    try {
        client::invoker_lock lock;
        
        try {
            // 再次确保目标被reveal（防止AI忘记）
            sqf::reveal(uav.game_object(), target.game_object());
        } catch (...) {
            // 忽略reveal失败
        }
        
        try {
            // 选择武器（如果指定）
            if (!weapon.empty()) {
                sqf::select_weapon(uav.game_object(), weapon);
            }
        } catch (...) {
            // 武器选择失败，使用默认武器
        }
        
        try {
            // 【关键修复4】使用 fireAtTarget 而不是 doFire
            // fireAtTarget 会让AI持续攻击目标直到摧毁或失去视线
            // doFire 只是一次性命令
            bool success = sqf::fire_at_target(uav.game_object(), target.game_object(), std::nullopt);
            
            if (!success) {
                // 备用方案：使用 commandFire
                try {
                    sqf::command_fire(uav.game_object(), target.game_object());
                } catch (...) {
                    return false;  // 两种方法都失败
                }
            }
        } catch (...) {
            // fireAtTarget失败，尝试commandFire
            try {
                sqf::command_fire(uav.game_object(), target.game_object());
            } catch (...) {
                return false;
            }
        }
        
        return true;
    } catch (...) {
        return false;
    }
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
