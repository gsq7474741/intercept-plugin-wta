#include "task_executor.hpp"
#include <intercept.hpp>
#include <algorithm>

namespace wta::exec {

using namespace intercept;

TaskExecutor::TaskExecutor() = default;

bool TaskExecutor::add_attack_task(const AttackTask& task) {
    // 检查 UAV 是否存在
    auto* uav = find_uav(task.platform_id);
    if (!uav || !uav->is_alive()) {
        return false;
    }
    
    // 检查 UAV 是否已有任务
    if (active_tasks_.find(task.platform_id) != active_tasks_.end()) {
        return false;  // 已有任务，拒绝
    }
    
    // 添加任务
    active_tasks_[task.platform_id] = task;
    active_tasks_[task.platform_id].started_time = AttackTask::clock::now();
    
    // 更新 UAV 状态
    uav->set_status(UavStatus::NavigatingToTarget);
    uav->set_current_task(task.target_id);
    
    // 更新统计
    stats_.on_task_created();
    
    return true;
}

bool TaskExecutor::cancel_task(wta::types::PlatformId platform_id) {
    auto it = active_tasks_.find(platform_id);
    if (it == active_tasks_.end()) {
        return false;
    }
    
    // 停止 UAV
    auto* uav = find_uav(platform_id);
    if (uav) {
        controller_.stop(*uav);
        uav->set_status(UavStatus::Idle);
        uav->clear_task();
    }
    
    // 移除任务
    active_tasks_.erase(it);
    stats_.on_task_failed();
    
    return true;
}

int TaskExecutor::tick() {
    if (active_tasks_.empty()) {
        return 0;
    }
    
    std::vector<wta::types::PlatformId> completed_tasks;
    
    // 【限流机制】将map转为vector以便轮询
    std::vector<wta::types::PlatformId> task_ids;
    task_ids.reserve(active_tasks_.size());
    for (const auto& [pid, _] : active_tasks_) {
        task_ids.push_back(pid);
    }
    
    // 【限流机制】确定本帧处理的任务范围
    size_t total_tasks = task_ids.size();
    size_t start_index = task_process_index_ % total_tasks;
    size_t tasks_to_process = (max_tasks_per_tick_ > 0) 
        ? std::min<size_t>(max_tasks_per_tick_, total_tasks)
        : total_tasks;
    
    // 【限流机制】轮询处理任务（避免总是处理前N个）
    for (size_t i = 0; i < tasks_to_process; ++i) {
        size_t idx = (start_index + i) % total_tasks;
        auto pid = task_ids[idx];
        
        auto it = active_tasks_.find(pid);
        if (it == active_tasks_.end()) continue;
        
        auto& task = it->second;
        process_task(task);
        
        // 检查任务是否完成或失败
        if (!task.is_active()) {
            completed_tasks.push_back(pid);
            
            // 更新统计
            if (task.stage == TaskStage::Completed) {
                stats_.on_task_completed();
            } else {
                stats_.on_task_failed();
            }
            
            // 重置 UAV 状态
            auto* uav = find_uav(pid);
            if (uav) {
                uav->set_status(UavStatus::Idle);
                uav->clear_task();
            }
        }
    }
    
    // 更新轮询索引
    task_process_index_ += tasks_to_process;
    
    // 清理完成的任务
    for (auto pid : completed_tasks) {
        active_tasks_.erase(pid);
    }
    
    return static_cast<int>(active_tasks_.size());
}

void TaskExecutor::register_uav(std::shared_ptr<UavEntity> uav) {
    if (uav) {
        uavs_[uav->id()] = uav;
    }
}

void TaskExecutor::register_target(std::shared_ptr<TargetEntity> target) {
    if (target) {
        targets_[target->id()] = target;
    }
}

void TaskExecutor::clear_all_tasks() {
    // 停止所有 UAV
    for (auto& [pid, task] : active_tasks_) {
        auto* uav = find_uav(pid);
        if (uav) {
            controller_.stop(*uav);
            uav->set_status(UavStatus::Idle);
            uav->clear_task();
        }
    }
    
    active_tasks_.clear();
}

void TaskExecutor::process_task(AttackTask& task) {
    switch (task.stage) {
        case TaskStage::Pending:
            task.stage = TaskStage::Navigate;
            [[fallthrough]];
        case TaskStage::Navigate:
            handle_navigate_stage(task);
            break;
        case TaskStage::Approach:
            handle_approach_stage(task);
            break;
        case TaskStage::Aiming:
            handle_aiming_stage(task);
            break;
        case TaskStage::Firing:
            handle_firing_stage(task);
            break;
        case TaskStage::Verify:
            handle_verify_stage(task);
            break;
        case TaskStage::Egress:
            handle_egress_stage(task);
            break;
        default:
            break;
    }
}

void TaskExecutor::handle_navigate_stage(AttackTask& task) {
    auto* uav = find_uav(task.platform_id);
    auto* target = find_target(task.target_id);
    
    if (!uav || !uav->is_alive()) {
        task.mark_failed();
        return;
    }
    
    if (!target || !target->is_alive()) {
        task.mark_completed();  // 目标已摧毁，任务完成
        return;
    }
    
    // 更新目标位置
    task.target_pos = target->position();
    
    // 发送导航命令
    if (!controller_.navigate_to(*uav, task.target_pos, 300.f)) {
        task.mark_failed();
        return;
    }
    
    // 检查是否到达接近距离
    if (controller_.has_reached(*uav, task.target_pos, task.approach_distance)) {
        sqf::diag_log("[WTA][TASK] UAV " + std::to_string(task.platform_id) + " reached approach distance, entering Approach stage");
        task.stage = TaskStage::Approach;
        uav->set_status(UavStatus::NavigatingToTarget);
    }
}

void TaskExecutor::handle_approach_stage(AttackTask& task) {
    auto* uav = find_uav(task.platform_id);
    auto* target = find_target(task.target_id);
    
    if (!uav || !uav->is_alive()) {
        task.mark_failed();
        return;
    }
    
    if (!target || !target->is_alive()) {
        task.mark_completed();
        return;
    }
    
    // 检查是否在交战距离内
    if (controller_.is_in_range(*uav, *target, task.engagement_distance)) {
        sqf::diag_log("[WTA][TASK] UAV " + std::to_string(task.platform_id) + " in engagement range, entering Aiming stage");
        task.stage = TaskStage::Aiming;
        uav->set_status(UavStatus::Aiming);
    } else {
        // 继续接近，并重新设置AI状态（防止被重置）
        task.target_pos = target->position();
        controller_.navigate_to(*uav, task.target_pos, 300.f);
    }
}

void TaskExecutor::handle_aiming_stage(AttackTask& task) {
    auto* uav = find_uav(task.platform_id);
    auto* target = find_target(task.target_id);
    
    if (!uav || !uav->is_alive()) {
        task.mark_failed();
        return;
    }
    
    if (!target || !target->is_alive()) {
        task.mark_completed();
        return;
    }
    
    // 先确保还在交战距离内（UAV可能飞过了）
    if (!controller_.is_in_range(*uav, *target, task.engagement_distance)) {
        sqf::diag_log("[WTA][TASK] UAV " + std::to_string(task.platform_id) + " out of range, returning to Approach");
        task.stage = TaskStage::Approach;
        return;
    }
    
    // 瞄准目标
    if (controller_.aim_at(*uav, *target)) {
        sqf::diag_log("[WTA][TASK] UAV " + std::to_string(task.platform_id) + " aimed, entering Firing stage");
        task.stage = TaskStage::Firing;
        uav->set_status(UavStatus::Firing);
    } else {
        // 瞄准失败，重试
        if (task.can_retry()) {
            task.mark_retry();
        } else {
            task.mark_failed();
        }
    }
}

void TaskExecutor::handle_firing_stage(AttackTask& task) {
    auto* uav = find_uav(task.platform_id);
    auto* target = find_target(task.target_id);
    
    if (!uav || !uav->is_alive()) {
        task.mark_failed();
        return;
    }
    
    if (!target || !target->is_alive()) {
        task.mark_completed();
        return;
    }
    
    // 选择武器
    if (task.weapon.empty()) {
        task.weapon = uav->get_best_weapon();
        sqf::diag_log("[WTA][TASK] UAV " + std::to_string(task.platform_id) + " selected weapon: " + task.weapon);
    }
    
    // 开火
    sqf::diag_log("[WTA][TASK] UAV " + std::to_string(task.platform_id) + " attempting to fire at target");
    if (controller_.fire_at(*uav, *target, task.weapon)) {
        sqf::diag_log("[WTA][TASK] UAV " + std::to_string(task.platform_id) + " fired, entering Verify stage");
        task.stage = TaskStage::Verify;
        uav->set_status(UavStatus::Firing);
    } else {
        // 开火失败，重试
        if (task.can_retry()) {
            task.mark_retry();
        } else {
            task.mark_failed();
        }
    }
}

void TaskExecutor::handle_verify_stage(AttackTask& task) {
    auto* target = find_target(task.target_id);
    
    // 等待一小段时间让武器到达目标
    if (task.elapsed_time() < 2.0) {
        return;  // 还没到验证时间
    }
    
    // 检查目标是否被摧毁
    if (is_target_destroyed(target)) {
        task.mark_completed();
    } else {
        // 目标未摧毁，进入撤离阶段
        task.stage = TaskStage::Egress;
    }
}

void TaskExecutor::handle_egress_stage(AttackTask& task) {
    auto* uav = find_uav(task.platform_id);
    
    if (!uav || !uav->is_alive()) {
        task.mark_failed();
        return;
    }
    
    // 命令撤离
    if (controller_.egress_to_safe(*uav)) {
        uav->set_status(UavStatus::Egressing);
        task.mark_completed();
    } else {
        task.mark_failed();
    }
}

UavEntity* TaskExecutor::find_uav(wta::types::PlatformId id) {
    auto it = uavs_.find(id);
    if (it != uavs_.end()) {
        return it->second.get();
    }
    return nullptr;
}

TargetEntity* TaskExecutor::find_target(wta::types::TargetId id) {
    auto it = targets_.find(id);
    if (it != targets_.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool TaskExecutor::is_target_destroyed(const TargetEntity* target) {
    if (!target) return true;
    return !target->is_alive();
}

} // namespace wta::exec
