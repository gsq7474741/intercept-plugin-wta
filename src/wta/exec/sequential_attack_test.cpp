#include "sequential_attack_test.hpp"
#include <intercept.hpp>
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>

namespace wta::test {

using namespace intercept;

namespace {

std::atomic<bool> g_running{false};

// ============================================================================
// 【新逻辑】所有UAV集中攻击当前目标，摧毁后立即切换到下一个目标
// ============================================================================

void run_test_loop() {
    g_running = true;

    wta::exec::EntityRegistry registry;
    wta::exec::TaskExecutor task_executor;

    // 第一次扫描实体
    if (!registry.discover_entities()) {
        {
            client::invoker_lock lock;
            sqf::diag_log("[WTA][TEST] discover_entities failed - is mission running?");
        }
        g_running = false;
        return;
    }

    // 注册 UAV 和目标
    for (const auto& pair : registry.get_uavs()) {
        task_executor.register_uav(pair.second);
    }
    for (const auto& pair : registry.get_targets()) {
        task_executor.register_target(pair.second);
    }

    if (registry.uav_count() == 0 || registry.target_count() == 0) {
        {
            client::invoker_lock lock;
            sqf::diag_log("[WTA][TEST] no UAVs or targets found, aborting test");
        }
        g_running = false;
        return;
    }

    // 收集 UAV ID
    std::vector<wta::types::PlatformId> uav_ids;
    for (const auto& pair : registry.get_uavs()) {
        uav_ids.push_back(pair.first);
    }
    std::sort(uav_ids.begin(), uav_ids.end());

    {
        client::invoker_lock lock;
        sqf::diag_log("[WTA][TEST] ========================================");
        sqf::diag_log("[WTA][TEST] Starting FOCUS FIRE test");
        sqf::diag_log("[WTA][TEST] All UAVs attack same target until destroyed");
        sqf::diag_log("[WTA][TEST] Then switch to next target");
        sqf::diag_log("[WTA][TEST] UAVs: " + std::to_string(uav_ids.size()));
        sqf::diag_log("[WTA][TEST] Targets: " + std::to_string(registry.target_count()));
        sqf::diag_log("[WTA][TEST] ========================================");
    }

    // 主循环：持续攻击直到所有目标被摧毁或无可用UAV
    int targets_destroyed = 0;
    
    while (g_running) {
        // ========== 步骤1：找到下一个存活目标 ==========
        std::shared_ptr<wta::exec::TargetEntity> current_target = nullptr;
        wta::types::TargetId current_target_id = 0;
        
        for (const auto& pair : registry.get_targets()) {
            if (pair.second && pair.second->is_alive()) {
                current_target = pair.second;
                current_target_id = pair.first;
                break;
            }
        }
        
        if (!current_target) {
            // 所有目标已摧毁
            {
                client::invoker_lock lock;
                sqf::diag_log("[WTA][TEST] 🎯🎯🎯 ALL TARGETS DESTROYED! Total: " + 
                             std::to_string(targets_destroyed));
            }
            break;
        }
        
        // ========== 步骤2：收集所有可用UAV ==========
        std::vector<wta::types::PlatformId> available_uavs;
        for (auto pid : uav_ids) {
            auto uav = registry.find_uav(pid);
            if (uav && uav->is_alive() && uav->has_ammo()) {
                available_uavs.push_back(pid);
            }
        }
        
        if (available_uavs.empty()) {
            {
                client::invoker_lock lock;
                sqf::diag_log("[WTA][TEST] ❌ No available UAVs (all dead or out of ammo)");
                sqf::diag_log("[WTA][TEST] Targets destroyed: " + std::to_string(targets_destroyed));
            }
            break;
        }
        
        {
            client::invoker_lock lock;
            sqf::diag_log("[WTA][TEST] ----------------------------------------");
            sqf::diag_log("[WTA][TEST] 🎯 Current target: " + std::to_string(current_target_id));
            sqf::diag_log("[WTA][TEST] 🚁 Available UAVs: " + std::to_string(available_uavs.size()));
            sqf::diag_log("[WTA][TEST] ----------------------------------------");
        }
        
        // ========== 步骤3：清除所有旧任务 ==========
        task_executor.clear_all_tasks();
        
        // ========== 步骤4：为所有可用UAV创建攻击任务 ==========
        auto target_pos = current_target->position();
        
        for (auto pid : available_uavs) {
            wta::exec::AttackTask task;
            task.platform_id = pid;
            task.target_id = current_target_id;
            task.target_pos = target_pos;
            
            if (!task_executor.add_attack_task(task)) {
                client::invoker_lock lock;
                sqf::diag_log("[WTA][TEST] Failed to add task for UAV " + std::to_string(pid));
            }
        }
        
        {
            client::invoker_lock lock;
            sqf::diag_log("[WTA][TEST] Created " + std::to_string(available_uavs.size()) + 
                         " attack tasks for target " + std::to_string(current_target_id));
        }
        
        // ========== 步骤5：执行任务直到目标被摧毁 ==========
        while (g_running) {
            // 驱动任务状态机
            task_executor.tick();
            
            // 检查目标是否被摧毁
            if (!current_target->is_alive()) {
                targets_destroyed++;
                {
                    client::invoker_lock lock;
                    sqf::diag_log("[WTA][TEST] 💥 Target " + std::to_string(current_target_id) + 
                                 " DESTROYED! (Total destroyed: " + std::to_string(targets_destroyed) + ")");
                }
                // 立即跳出，切换到下一个目标
                break;
            }
            
            // 检查是否还有活跃任务
            if (task_executor.active_task_count() == 0) {
                // 所有任务完成但目标未摧毁，重新分配任务
                {
                    client::invoker_lock lock;
                    sqf::diag_log("[WTA][TEST] All tasks completed but target still alive, reassigning...");
                }
                break;  // 跳出内循环，重新分配任务
            }
            
            // 控制 tick 频率
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        // 短暂延迟再处理下一个目标
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    {
        client::invoker_lock lock;
        sqf::diag_log("[WTA][TEST] ========================================");
        sqf::diag_log("[WTA][TEST] FOCUS FIRE test completed");
        sqf::diag_log("[WTA][TEST] Targets destroyed: " + std::to_string(targets_destroyed));
        sqf::diag_log("[WTA][TEST] ========================================");
    }
    g_running = false;
}

} // namespace

void start_sequential_attack_test() {
    if (g_running) return;
    std::thread th(run_test_loop);
    th.detach();
}

void stop_sequential_attack_test() {
    g_running = false;
}

bool is_test_running() {
    return g_running;
}

} // namespace wta::test
