#pragma once
#include "uav_entity.hpp"
#include "task.hpp"
#include <memory>

namespace wta::exec {

/**
 * @brief UAV 控制器 - 负责发送具体的游戏控制命令
 */
class UavController {
public:
    UavController() = default;
    
    /**
     * @brief 命令 UAV 导航到目标位置
     * @param uav UAV实体
     * @param target_pos 目标位置
     * @param altitude 飞行高度（米）
     * @return 是否成功发送命令
     */
    bool navigate_to(UavEntity& uav, const wta::types::Vec2& target_pos, float altitude = 300.f);
    
    /**
     * @brief 命令 UAV 瞄准目标
     * @param uav UAV实体
     * @param target 目标实体
     * @return 是否成功瞄准
     */
    bool aim_at(UavEntity& uav, const TargetEntity& target);
    
    /**
     * @brief 命令 UAV 开火
     * @param uav UAV实体
     * @param target 目标实体
     * @param weapon 武器名称
     * @return 是否成功开火
     */
    bool fire_at(UavEntity& uav, const TargetEntity& target, const std::string& weapon);
    
    /**
     * @brief 命令 UAV 撤离到安全位置
     * @param uav UAV实体
     * @param safe_pos 安全位置（可选，如果不提供则自动计算）
     * @return 是否成功发送命令
     */
    bool egress_to_safe(UavEntity& uav, const wta::types::Vec2* safe_pos = nullptr);
    
    /**
     * @brief 命令 UAV 返回基地
     * @param uav UAV实体
     * @return 是否成功发送命令
     */
    bool return_to_base(UavEntity& uav);
    
    /**
     * @brief 检查 UAV 是否到达目标位置
     * @param uav UAV实体
     * @param target_pos 目标位置
     * @param tolerance 容差（米）
     * @return 是否到达
     */
    bool has_reached(const UavEntity& uav, const wta::types::Vec2& target_pos, float tolerance = 50.f) const;
    
    /**
     * @brief 检查 UAV 是否在交战距离内
     * @param uav UAV实体
     * @param target 目标实体
     * @param engagement_distance 交战距离（米）
     * @return 是否在交战距离内
     */
    bool is_in_range(const UavEntity& uav, const TargetEntity& target, float engagement_distance) const;
    
    /**
     * @brief 停止 UAV 的所有移动
     * @param uav UAV实体
     */
    void stop(UavEntity& uav);
    
    // ========== 【新增】仿照 fn_execution.sqf 的函数 ==========
    
    /**
     * @brief 设置攻击高度（根据武器类型自动选择）
     * 仿照 fn_execution.sqf 的 flyInHeight 逻辑
     * - Missile: 200m
     * - Bomb: 150m
     * - Rocket: 100m
     * @param uav UAV实体
     * @param weapon 武器名称
     * @return 是否成功设置
     */
    bool set_attack_height(UavEntity& uav, const std::string& weapon);
    
    /**
     * @brief 获取 UAV 的总弹药数量（排除非消耗性物品）
     * 仿照 fn_execution.sqf 的 magazinesAmmo 逻辑
     * @param uav UAV实体
     * @return 弹药总数
     */
    int get_total_ammo(const UavEntity& uav) const;
    
    /**
     * @brief 检查弹药是否被消耗（用于验证开火成功）
     * 仿照 fn_execution.sqf 的弹药检测逻辑
     * @param uav UAV实体
     * @param ammo_before 开火前的弹药数量
     * @return 是否有弹药被消耗
     */
    bool check_ammo_consumed(const UavEntity& uav, int ammo_before) const;
    
    // ========== 【激光目标】绕过 AI 射击限制 ==========
    
    /**
     * @brief 为目标创建激光标记
     * 创建一个 LaserTarget 对象并附加到目标上
     * 激光制导武器（Scalpel、GBU-12等）会自动追踪该激光目标
     * @param target 目标实体
     */
    void create_laser_target(const TargetEntity& target);
    
    /**
     * @brief 移除目标的激光标记
     * @param target 目标实体
     */
    void remove_laser_target(const TargetEntity& target);
    
private:
    // 计算两点之间的距离
    float distance(const wta::types::Vec2& a, const wta::types::Vec2& b) const;
    
    // 计算安全撤离位置
    wta::types::Vec2 calculate_safe_egress_position(const UavEntity& uav, const wta::types::Vec2& threat_pos) const;
};

} // namespace wta::exec
