#include "orchestrator.hpp"
#include <chrono>

namespace wta::orch {

using clock = std::chrono::steady_clock;

static inline double now_sec() {
    return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

void Orchestrator::start() {
    if (running_.exchange(true)) return;
    th_reporter_ = std::thread(&Orchestrator::loop_reporter, this);
    th_solver_ = std::thread(&Orchestrator::loop_solver, this);
    th_executor_ = std::thread(&Orchestrator::loop_executor, this);
}

void Orchestrator::stop() {
    if (!running_.exchange(false)) return;
    if (th_reporter_.joinable()) th_reporter_.join();
    if (th_solver_.joinable()) th_solver_.join();
    if (th_executor_.joinable()) th_executor_.join();
}

bool Orchestrator::need_replan(double now) const {
    if (pending_replan_) return true;
    if (last_resp_) {
        const double age = now - last_solve_ts_;
        if (age >= ttl_sec_) return true;
    } else {
        return true;
    }
    if (now < next_allowed_solve_ts_) return false;
    return false;
}

// 数据上报循环：持续采样并发送数据给前端（不触发规划）
void Orchestrator::loop_reporter() {
    using namespace std::chrono_literals;
    while (running_) {
        const double t = now_sec();
        
        // 采样当前战场状态
        wta::proto::StatusReportEvent event{};
        event.timestamp = t;
        
        // 使用临时request来采样（复用sampler接口）
        wta::proto::SolveRequest temp_req{};
        temp_req.timestamp = t;
        sampler_.sample(temp_req);
        
        // 转换为StatusReport格式
        event.platforms = std::move(temp_req.platforms);
        event.targets = std::move(temp_req.targets);
        
        // 上报状态（fire-and-forget）
        client_.report_status(event, 500ms);
        
        // 每秒上报一次
        std::this_thread::sleep_for(1000ms);
    }
}

// 规划循环：根据事件和TTL决定是否重新规划任务
void Orchestrator::loop_solver() {
    using namespace std::chrono_literals;
    while (running_) {
        // Drain events quickly and mark replan when needed
        {
            wta::events::Event ev{};
            int drained = 0;
            while (bus_.try_pop(ev) && drained < 128) {
                switch (ev.type) {
                    case wta::events::EventType::EntityKilled:
                    case wta::events::EventType::HandleDamage:
                    case wta::events::EventType::Fired:
                    case wta::events::EventType::ReplanRequest:
                        pending_replan_ = true; break;
                    default: break;
                }
                ++drained;
            }
        }

        const double t = now_sec();
        if (need_replan(t) && t >= next_allowed_solve_ts_) {
            // 采样当前状态
            wta::proto::SolveRequest temp_req{};
            temp_req.timestamp = t;
            sampler_.sample(temp_req);
            
            // 构建规划请求
            wta::proto::PlanRequest plan_req{};
            plan_req.timestamp = t;
            plan_req.reason = pending_replan_ ? "event_triggered" : "ttl_expired";
            plan_req.platforms = std::move(temp_req.platforms);
            plan_req.targets = std::move(temp_req.targets);

            wta::proto::PlanResponse plan_resp{};
            const auto ok = client_.request_plan(plan_req, plan_resp, 1000ms);
            if (ok && plan_resp.status == "ok") {
                // 转换为旧格式供executor使用（临时兼容）
                wta::proto::SolveResponse legacy_resp{};
                legacy_resp.status = plan_resp.status;
                legacy_resp.assignment = std::move(plan_resp.assignment);
                legacy_resp.ttl_sec = plan_resp.ttl_sec;
                
                last_resp_ = legacy_resp;
                last_solve_ts_ = t;
                ttl_sec_ = plan_resp.ttl_sec;
                next_allowed_solve_ts_ = t + 0.5; // 节流窗口
                pending_replan_ = false;
                exec_.apply_assignment(legacy_resp);
            } else {
                // 失败：维持旧解，稍后重试
                next_allowed_solve_ts_ = t + 0.5;
            }
        }
        std::this_thread::sleep_for(50ms);
    }
}

void Orchestrator::loop_executor() {
    using namespace std::chrono_literals;
    while (running_) {
        exec_.tick();
        std::this_thread::sleep_for(50ms);
    }
}

} // namespace wta::orch
