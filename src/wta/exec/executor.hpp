#pragma once
#include <memory>
#include "../core/solver_messages.hpp"

namespace wta::exec {

enum class ActionStage : uint8_t { Navigate, Aim, Fire, Egress };

struct EngagementAction {
    wta::types::PlatformId pid;
    wta::types::TargetId   tid;
    std::string weapon; // "missile"/"bomb"/"rocket"
    ActionStage stage{ActionStage::Navigate};
    int retries{0};
};

struct IExecutor {
    virtual ~IExecutor() = default;
    virtual void apply_assignment(const wta::proto::PlanResponse& resp) = 0;
    virtual void tick() = 0;
};

std::unique_ptr<IExecutor> make_intercept_executor();

} // namespace wta::exec
