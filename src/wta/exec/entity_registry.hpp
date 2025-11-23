#pragma once
#include "uav_entity.hpp"
#include <memory>
#include <unordered_map>
#include <vector>

namespace intercept::types {
    class object;
}

namespace wta::exec {

/**
 * @brief 实体注册表 - 从游戏中自动发现并管理 UAV 和目标
 */
class EntityRegistry {
public:
    EntityRegistry() = default;
    
    /**
     * @brief 从游戏中扫描并注册所有实体
     * @return 是否成功扫描
     */
    bool discover_entities();
    
    /**
     * @brief 获取所有 UAV 实体
     */
    const std::unordered_map<wta::types::PlatformId, std::shared_ptr<UavEntity>>& get_uavs() const {
        return uavs_;
    }
    
    /**
     * @brief 获取所有目标实体
     */
    const std::unordered_map<wta::types::TargetId, std::shared_ptr<TargetEntity>>& get_targets() const {
        return targets_;
    }
    
    /**
     * @brief 根据 ID 查找 UAV
     */
    std::shared_ptr<UavEntity> find_uav(wta::types::PlatformId id) const;
    
    /**
     * @brief 根据 ID 查找目标
     */
    std::shared_ptr<TargetEntity> find_target(wta::types::TargetId id) const;
    
    /**
     * @brief 清空所有实体
     */
    void clear();
    
    /**
     * @brief 获取统计信息
     */
    int uav_count() const { return static_cast<int>(uavs_.size()); }
    int target_count() const { return static_cast<int>(targets_.size()); }
    
    /**
     * @brief 更新实体状态（移除已摧毁的实体）
     */
    void update();
    
private:
    // 从游戏对象创建 UAV 实体
    std::shared_ptr<UavEntity> create_uav_from_object(
        wta::types::PlatformId id, 
        const intercept::types::object& obj);
    
    // 从游戏对象创建目标实体
    std::shared_ptr<TargetEntity> create_target_from_object(
        wta::types::TargetId id,
        const intercept::types::object& obj);
    
    // 检查对象是否是友方 UAV
    bool is_friendly_uav(const intercept::types::object& obj) const;
    
    // 检查对象是否是敌方单位
    bool is_enemy(const intercept::types::object& obj) const;
    
    // 获取或分配目标 ID
    int get_or_assign_target_id(const intercept::types::object& obj);
    
    std::unordered_map<wta::types::PlatformId, std::shared_ptr<UavEntity>> uavs_;
    std::unordered_map<wta::types::TargetId, std::shared_ptr<TargetEntity>> targets_;
    
    // 用于跟踪游戏对象到 ID 的映射
    std::unordered_map<std::string, int> object_to_target_id_;
    int next_platform_id_{1};
    int next_target_id_{1};
};

} // namespace wta::exec
