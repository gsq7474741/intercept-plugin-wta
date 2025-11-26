#include "entity_registry.hpp"
#include <intercept.hpp>
#include <sstream>

namespace wta::exec {

using namespace intercept;

bool EntityRegistry::discover_entities() {
    try {
        client::invoker_lock lock;
        
        // 清空旧数据
        uavs_.clear();
        targets_.clear();
        
        // 获取玩家阵营
        auto player = sqf::player();
        if (player.is_null()) {
            return false;  // 游戏未准备好
        }
        
        auto player_side = sqf::get_side(player);
        
        // ===== 扫描所有载具 =====
        auto vehicles = sqf::vehicles();
        
        for (const auto& veh : vehicles) {
            if (veh.is_null() || !sqf::alive(veh)) continue;
            
            auto veh_side = sqf::get_side(veh);
            bool is_uav = sqf::unit_is_uav(veh);
            
            // 友方 UAV → 注册为平台
            if (veh_side == player_side && is_uav) {
                auto uav = create_uav_from_object(next_platform_id_, veh);
                if (uav) {
                    uavs_[next_platform_id_] = uav;
                    next_platform_id_++;
                }
            }
            // 敌方载具 → 注册为目标
            else if (veh_side != player_side && veh_side != sqf::civilian()) {
                int target_id = get_or_assign_target_id(veh);
                auto target = create_target_from_object(target_id, veh);
                if (target) {
                    targets_[target_id] = target;
                    if (target_id >= next_target_id_) {
                        next_target_id_ = target_id + 1;
                    }
                }
            }
        }
        
        // ===== 扫描所有步兵单位 =====
        auto units = sqf::all_units();
        
        for (const auto& unit : units) {
            if (unit.is_null() || !sqf::alive(unit)) continue;
            
            auto unit_side = sqf::get_side(unit);
            
            // 只采集敌方步兵作为目标
            if (unit_side != player_side && unit_side != sqf::civilian()) {
                int target_id = get_or_assign_target_id(unit);
                auto target = create_target_from_object(target_id, unit);
                if (target) {
                    targets_[target_id] = target;
                    if (target_id >= next_target_id_) {
                        next_target_id_ = target_id + 1;
                    }
                }
            }
        }
        
        return true;
        
    } catch (...) {
        return false;
    }
}

std::shared_ptr<UavEntity> EntityRegistry::find_uav(wta::types::PlatformId id) const {
    auto it = uavs_.find(id);
    if (it != uavs_.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<TargetEntity> EntityRegistry::find_target(wta::types::TargetId id) const {
    auto it = targets_.find(id);
    if (it != targets_.end()) {
        return it->second;
    }
    return nullptr;
}

void EntityRegistry::clear() {
    uavs_.clear();
    targets_.clear();
    object_to_target_id_.clear();
    next_platform_id_ = 1;
    next_target_id_ = 1;
}

void EntityRegistry::update() {
    try {
        client::invoker_lock lock;
        
        // 移除已摧毁的 UAV
        std::vector<wta::types::PlatformId> dead_uavs;
        for (const auto& [id, uav] : uavs_) {
            if (!uav->is_alive()) {
                dead_uavs.push_back(id);
            }
        }
        for (auto id : dead_uavs) {
            uavs_.erase(id);
        }
        
        // 移除已摧毁的目标
        std::vector<wta::types::TargetId> dead_targets;
        for (const auto& [id, target] : targets_) {
            if (!target->is_alive()) {
                dead_targets.push_back(id);
            }
        }
        for (auto id : dead_targets) {
            targets_.erase(id);
        }
        
    } catch (...) {
        // 忽略错误
    }
}

std::shared_ptr<UavEntity> EntityRegistry::create_uav_from_object(
    wta::types::PlatformId id,
    const intercept::types::object& obj) {
    
    try {
        return std::make_shared<UavEntity>(id, obj);
    } catch (...) {
        return nullptr;
    }
}

std::shared_ptr<TargetEntity> EntityRegistry::create_target_from_object(
    wta::types::TargetId id,
    const intercept::types::object& obj) {
    
    try {
        client::invoker_lock lock;  // 【线程安全】必须加锁
        
        auto target = std::make_shared<TargetEntity>(id, obj);
        
        // 根据目标类型设置威胁等级
        auto type_name = sqf::type_of(obj);
        
        if (type_name.find("AA") != std::string::npos ||
            type_name.find("SAM") != std::string::npos ||
            type_name.find("Radar") != std::string::npos) {
            target->set_threat_level(1.0f);  // 高威胁
        } else if (type_name.find("Tank") != std::string::npos ||
                   type_name.find("APC") != std::string::npos) {
            target->set_threat_level(0.7f);  // 中等威胁
        } else {
            target->set_threat_level(0.3f);  // 低威胁
        }
        
        return target;
    } catch (...) {
        return nullptr;
    }
}

bool EntityRegistry::is_friendly_uav(const intercept::types::object& obj) const {
    try {
        client::invoker_lock lock;
        
        auto player = sqf::player();
        if (player.is_null()) return false;
        
        auto player_side = sqf::get_side(player);
        auto obj_side = sqf::get_side(obj);
        
        return obj_side == player_side && sqf::unit_is_uav(obj);
    } catch (...) {
        return false;
    }
}

bool EntityRegistry::is_enemy(const intercept::types::object& obj) const {
    try {
        client::invoker_lock lock;
        
        auto player = sqf::player();
        if (player.is_null()) return false;
        
        auto player_side = sqf::get_side(player);
        auto obj_side = sqf::get_side(obj);
        
        return obj_side != player_side && obj_side != sqf::civilian();
    } catch (...) {
        return false;
    }
}

int EntityRegistry::get_or_assign_target_id(const intercept::types::object& obj) {
    try {
        client::invoker_lock lock;  // 【线程安全】必须加锁
        
        // 首先检查对象是否有 wta_target_id 变量
        auto target_id_var = sqf::get_variable(obj, "wta_target_id");
        if (!target_id_var.is_nil()) {
            float id_float = static_cast<float>(target_id_var);
            return static_cast<int>(id_float);
        }
        
        // 使用对象的 netId 作为唯一标识
        auto net_id = sqf::net_id(obj);
        
        // 检查是否已分配 ID
        auto it = object_to_target_id_.find(net_id);
        if (it != object_to_target_id_.end()) {
            return it->second;
        }
        
        // 分配新 ID
        int new_id = next_target_id_;
        object_to_target_id_[net_id] = new_id;
        
        return new_id;
        
    } catch (...) {
        return next_target_id_;
    }
}

} // namespace wta::exec
