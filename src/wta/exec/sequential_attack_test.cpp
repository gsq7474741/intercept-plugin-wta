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

void run_test_loop() {
    g_running = true;

    wta::exec::EntityRegistry registry;
    wta::exec::TaskExecutor task_executor;

    // 第一次扫描实体
    if (!registry.discover_entities()) {
        sqf::diag_log("[WTA][TEST] discover_entities failed - is mission running?");
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
        sqf::diag_log("[WTA][TEST] no UAVs or targets found, aborting test");
        g_running = false;
        return;
    }

    // 收集并排序 UAV / Target ID，保证顺序稳定
    std::vector<wta::types::PlatformId> uav_ids;
    std::vector<wta::types::TargetId> target_ids;

    for (const auto& pair : registry.get_uavs()) {
        uav_ids.push_back(pair.first);
    }
    for (const auto& pair : registry.get_targets()) {
        target_ids.push_back(pair.first);
    }

    std::sort(uav_ids.begin(), uav_ids.end());
    std::sort(target_ids.begin(), target_ids.end());

    sqf::diag_log("[WTA][TEST] starting sequential attack test - ALL UAVs attack SAME target");

    // 按目标顺序，让所有 UAV 同时攻击同一个目标
    for (auto tid : target_ids) {
        if (!g_running) break;

        auto target = registry.find_target(tid);
        if (!target || !target->is_alive()) {
            sqf::diag_log("[WTA][TEST] target already destroyed, skipping");
            continue;
        }

        sqf::diag_log("[WTA][TEST] assigning ALL available UAVs to target " + std::to_string(tid));

        // 收集所有存活且有弹药的 UAV，全部分配给当前目标
        std::vector<wta::types::PlatformId> available_uavs;
        for (auto pid : uav_ids) {
            auto uav = registry.find_uav(pid);
            if (uav && uav->is_alive() && uav->has_ammo()) {
                available_uavs.push_back(pid);
            }
        }

        if (available_uavs.empty()) {
            sqf::diag_log("[WTA][TEST] no available UAVs, stopping test");
            break;
        }

        sqf::diag_log("[WTA][TEST] dispatching " + std::to_string(available_uavs.size()) + " UAVs in batches");

        // 【防崩溃】分批创建任务，每批2个，间隔500ms
        const size_t batch_size = 2;
        for (size_t batch_start = 0; batch_start < available_uavs.size(); batch_start += batch_size) {
            size_t batch_end = std::min(batch_start + batch_size, available_uavs.size());
            
            sqf::diag_log("[WTA][TEST] batch " + std::to_string(batch_start/batch_size + 1) + 
                         ": creating tasks for UAV " + std::to_string(batch_start+1) + 
                         "-" + std::to_string(batch_end));
            
            // 为本批次的 UAV 创建攻击任务
            for (size_t i = batch_start; i < batch_end; ++i) {
                auto pid = available_uavs[i];
                wta::exec::AttackTask task;
                task.platform_id = pid;
                task.target_id = tid;

                if (!task_executor.add_attack_task(task)) {
                    sqf::diag_log("[WTA][TEST] failed to add task for UAV " + std::to_string(pid));
                }
            }
            
            // 批次间延迟，让引擎有时间处理
            if (batch_end < available_uavs.size()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
        
        sqf::diag_log("[WTA][TEST] all tasks created, starting execution");

        // 驱动状态机直到所有任务完成或目标被摧毁
        while (g_running && task_executor.active_task_count() > 0) {
            task_executor.tick();
            
            // 检查目标是否已被摧毁
            auto target_check = registry.find_target(tid);
            if (!target_check || !target_check->is_alive()) {
                sqf::diag_log("[WTA][TEST] target " + std::to_string(tid) + " destroyed, moving to next target");
                // 取消所有针对该目标的任务
                for (auto pid : available_uavs) {
                    task_executor.cancel_task(pid);
                }
                break;
            }
            
            // 【修复】降低tick频率：从100ms改为500ms，给引擎更多缓冲时间
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        sqf::diag_log("[WTA][TEST] finished attacking target " + std::to_string(tid));
    }

    sqf::diag_log("[WTA][TEST] sequential attack test completed");
    g_running = false;
}

} // namespace

void start_sequential_attack_test() {
    if (g_running) return;
    std::thread th(run_test_loop);
    th.detach();
}

} // namespace wta::test
