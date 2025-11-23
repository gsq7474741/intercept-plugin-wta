#include "executor.hpp"
#include "task_executor.hpp"
#include "entity_registry.hpp"
#include <intercept.hpp>
#include <mutex>
#include <memory>

namespace wta::exec {

using namespace intercept;

/**
 * @brief Intercept 执行器实现 - 将 WTA 分配结果转换为具体的 UAV 控制任务
 */
class InterceptExecutor final : public IExecutor {
public:
    InterceptExecutor() {
        // 初始化组件
        task_executor_ = std::make_unique<TaskExecutor>();
        entity_registry_ = std::make_unique<EntityRegistry>();
    }
    
    void apply_assignment(const wta::proto::PlanResponse& resp) override {
        std::lock_guard<std::mutex> lk(m_);
        
        // 发现并注册所有实体
        if (entity_registry_->discover_entities()) {
            // 将实体注册到任务执行器
            for (const auto& [id, uav] : entity_registry_->get_uavs()) {
                task_executor_->register_uav(uav);
            }
            for (const auto& [id, target] : entity_registry_->get_targets()) {
                task_executor_->register_target(target);
            }
        }
        
        // 清空旧任务
        task_executor_->clear_all_tasks();
        
        // 解析分配矩阵并创建任务
        const size_t n_plat = resp.n_platforms;
        const size_t n_tgt = resp.n_targets;
        
        for (size_t i = 0; i < n_plat; ++i) {
            for (size_t j = 0; j < n_tgt; ++j) {
                const size_t idx = i * n_tgt + j;
                if (idx < resp.assignment.size() && resp.assignment[idx] > 0) {
                    // 创建攻击任务
                    AttackTask task;
                    task.platform_id = static_cast<wta::types::PlatformId>(i + 1);
                    task.target_id = static_cast<wta::types::TargetId>(j + 1);
                    // 目标位置会在任务执行时动态更新
                    
                    task_executor_->add_attack_task(task);
                }
            }
        }
    }
    
    void tick() override {
        std::lock_guard<std::mutex> lk(m_);
        
        // 更新实体状态（移除已摧毁的）
        if (entity_registry_) {
            entity_registry_->update();
        }
        
        // 执行任务
        if (task_executor_) {
            task_executor_->tick();
        }
    }
    
private:
    std::mutex m_;
    std::unique_ptr<TaskExecutor> task_executor_;
    std::unique_ptr<EntityRegistry> entity_registry_;
};

std::unique_ptr<IExecutor> make_intercept_executor() {
    return std::make_unique<InterceptExecutor>();
}

} // namespace wta::exec
