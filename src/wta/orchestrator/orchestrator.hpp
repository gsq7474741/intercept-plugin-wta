#pragma once
#include <atomic>
#include <optional>
#include <thread>
#include "../world/event_bus.hpp"
#include "../net/solver_client.hpp"
#include "../world/world_sampler.hpp"
#include "../exec/executor.hpp"

namespace wta::orch {

class Orchestrator {
public:
    Orchestrator(wta::events::EventBus& bus,
                 wta::net::ISolverClient& client,
                 wta::world::IWorldSampler& sampler,
                 wta::exec::IExecutor& executor)
    : bus_(bus), client_(client), sampler_(sampler), exec_(executor) {}

    void start();
    void stop();

private:
    void loop_reporter();   // 持续上报数据给前端
    void loop_solver();     // 规划任务分配
    void loop_executor();   // 执行任务
    bool need_replan(double now) const;

    std::atomic<bool> running_{false};
    std::thread th_reporter_;   // 数据上报线程
    std::thread th_solver_;     // 规划线程
    std::thread th_executor_;   // 执行线程

    wta::events::EventBus& bus_;
    wta::net::ISolverClient& client_;
    wta::world::IWorldSampler& sampler_;
    wta::exec::IExecutor& exec_;

    std::optional<wta::proto::SolveResponse> last_resp_{};
    double last_solve_ts_{0.0};
    double next_allowed_solve_ts_{0.0};
    double ttl_sec_{2.0};
    bool pending_replan_{true};
};

} // namespace wta::orch
