#include "orchestrator.hpp"
#include <chrono>

namespace wta::orch {

using clock = std::chrono::steady_clock;

static inline double now_sec() {
    return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

void Orchestrator::start() {
    if (running_.exchange(true)) return;
    th_solver_ = std::thread(&Orchestrator::loop_solver, this);
    th_executor_ = std::thread(&Orchestrator::loop_executor, this);
}

void Orchestrator::stop() {
    if (!running_.exchange(false)) return;
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
            wta::proto::SolveRequest req{};
            req.mission_id = "default";
            req.timestamp = t;
            sampler_.sample(req);

            wta::proto::SolveResponse resp{};
            const auto ok = client_.solve(req, resp, std::chrono::milliseconds(1000));
            if (ok && resp.status == "ok") {
                last_resp_ = resp;
                last_solve_ts_ = t;
                ttl_sec_ = resp.ttl_sec;
                next_allowed_solve_ts_ = t + 0.5; // 节流窗口
                pending_replan_ = false;
                exec_.apply_assignment(resp);
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
