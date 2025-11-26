#pragma once
#include <string>
#include <chrono>
#include "../core/types.hpp"

namespace wta::exec {

/**
 * @brief 任务类型
 */
enum class TaskType : uint8_t {
    Attack,         // 攻击任务
    Recon,          // 侦察任务
    RTB,            // 返回基地
    Idle            // 空闲
};

/**
 * @brief 任务阶段
 */
enum class TaskStage : uint8_t {
    Pending,        // 等待执行
    Navigate,       // 导航到目标
    Approach,       // 接近目标
    Aiming,         // 瞄准目标
    Firing,         // 开火
    Verify,         // 验证毁伤
    Egress,         // 撤离
    Completed,      // 完成
    Failed          // 失败
};

/**
 * @brief 攻击任务
 */
struct AttackTask {
    wta::types::PlatformId platform_id{0};  // 执行平台ID
    wta::types::TargetId target_id{0};      // 目标ID
    wta::types::Vec2 target_pos{};          // 目标位置
    std::string weapon;                     // 武器类型
    TaskStage stage{TaskStage::Pending};    // 当前阶段
    
    // 执行参数
    // 【优化】减小距离以提高武器精度
    float approach_distance{600.f};         // 接近距离（米）- 更近以便瞄准
    float engagement_distance{400.f};       // 交战距离（米）- 更近才开火，提高精度
    int max_retries{10};                    // 最大重试次数
    int retry_count{0};                     // 当前重试次数
    
    // 【新增】仿照 fn_execution.sqf 的弹药跟踪
    int ammo_before_fire{0};                // 开火前弹药数量
    
    // 时间戳
    using clock = std::chrono::steady_clock;
    clock::time_point created_time;
    clock::time_point started_time;
    clock::time_point completed_time;
    clock::time_point fire_command_time;    // 发送开火命令的时间
    
    AttackTask() : created_time(clock::now()) {}
    
    // 状态查询
    bool is_active() const {
        return stage != TaskStage::Completed && stage != TaskStage::Failed;
    }
    
    bool can_retry() const {
        return retry_count < max_retries;
    }
    
    void mark_retry() {
        retry_count++;
        stage = TaskStage::Navigate;
    }
    
    void mark_completed() {
        stage = TaskStage::Completed;
        completed_time = clock::now();
    }
    
    void mark_failed() {
        stage = TaskStage::Failed;
        completed_time = clock::now();
    }
    
    // 获取执行时长（秒）
    double elapsed_time() const {
        if (started_time == clock::time_point{}) return 0.0;
        auto now = clock::now();
        return std::chrono::duration<double>(now - started_time).count();
    }
};

/**
 * @brief 任务状态统计
 */
struct TaskStatistics {
    int total_tasks{0};
    int completed_tasks{0};
    int failed_tasks{0};
    int active_tasks{0};
    
    void on_task_created() { total_tasks++; active_tasks++; }
    void on_task_completed() { completed_tasks++; active_tasks--; }
    void on_task_failed() { failed_tasks++; active_tasks--; }
    
    float success_rate() const {
        if (total_tasks == 0) return 0.f;
        return static_cast<float>(completed_tasks) / total_tasks;
    }
};

} // namespace wta::exec
