#include "executor.hpp"
#include <queue>
#include <mutex>
#include <memory>

namespace wta::exec {

class InterceptExecutor final : public IExecutor {
public:
    void apply_assignment(const wta::proto::SolveResponse& resp) override {
        std::lock_guard<std::mutex> lk(m_);
        pending_ = true;
        n_plat_ = resp.n_platforms;
        n_tgt_  = resp.n_targets;
        assign_ = resp.assignment;
    }
    void tick() override {
        std::lock_guard<std::mutex> lk(m_);
        if (!pending_) return;
        pending_ = false;
    }
private:
    std::mutex m_;
    bool pending_{false};
    size_t n_plat_{0};
    size_t n_tgt_{0};
    wta::types::AssignmentMatrix assign_{};
};

std::unique_ptr<IExecutor> make_intercept_executor() {
    return std::make_unique<InterceptExecutor>();
}

} // namespace wta::exec
