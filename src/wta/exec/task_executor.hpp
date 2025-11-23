#pragma once
#include "uav_entity.hpp"
#include "task.hpp"
#include "uav_controller.hpp"
#include <unordered_map>
#include <vector>
#include <memory>

namespace wta::exec {

/**
 * @brief 任务执行器 - 实现任务状态机和执行逻辑
 */
class TaskExecutor {
public:
    TaskExecutor();
    
    /**
     * @brief 添加新的攻击任务
     * @param task 任务描述
     * @return 是否成功添加
     */
    bool add_attack_task(const AttackTask& task);
    
    /**
     * @brief 取消指定平台的任务
     * @param platform_id 平台ID
     * @return 是否成功取消
     */
    bool cancel_task(wta::types::PlatformId platform_id);
    
    /**
     * @brief 执行一次状态机更新（在主循环中调用）
     * @return 处理的任务数量
     */
    int tick();
    
    /**
     * @brief 注册 UAV 实体
     * @param uav UAV实体（智能指针）
     */
    void register_uav(std::shared_ptr<UavEntity> uav);
    
    /**
     * @brief 注册目标实体
     * @param target 目标实体（智能指针）
     */
    void register_target(std::shared_ptr<TargetEntity> target);
    
    /**
     * @brief 获取任务统计
     */
    const TaskStatistics& statistics() const { return stats_; }
    
    /**
     * @brief 清空所有任务
     */
    void clear_all_tasks();
    
    /**
     * @brief 获取活跃任务数量
     */
    int active_task_count() const { return static_cast<int>(active_tasks_.size()); }
    
    /**
     * @brief 设置每帧最大处理任务数（限流）
     * @param max_tasks 最大任务数，0表示不限制
     */
    void set_max_tasks_per_tick(int max_tasks) { max_tasks_per_tick_ = max_tasks; }
    
private:
    // 状态机：处理单个任务的一次迭代
    void process_task(AttackTask& task);
    
    // 各阶段处理函数
    void handle_navigate_stage(AttackTask& task);
    void handle_approach_stage(AttackTask& task);
    void handle_aiming_stage(AttackTask& task);
    void handle_firing_stage(AttackTask& task);
    void handle_verify_stage(AttackTask& task);
    void handle_egress_stage(AttackTask& task);
    
    // 辅助函数
    UavEntity* find_uav(wta::types::PlatformId id);
    TargetEntity* find_target(wta::types::TargetId id);
    bool is_target_destroyed(const TargetEntity* target);
    
    // 数据成员
    UavController controller_;
    std::unordered_map<wta::types::PlatformId, std::shared_ptr<UavEntity>> uavs_;
    std::unordered_map<wta::types::TargetId, std::shared_ptr<TargetEntity>> targets_;
    std::unordered_map<wta::types::PlatformId, AttackTask> active_tasks_;
    TaskStatistics stats_;
    
    // 限流机制：防止同时处理过多UAV导致引擎崩溃
    int max_tasks_per_tick_ = 2;  // 每帧最多处理2个任务（更安全）
    size_t task_process_index_ = 0;  // 轮询索引
};

} // namespace wta::exec
