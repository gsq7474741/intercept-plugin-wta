#pragma once
#include <chrono>
#include <memory>
#include <string>
#include "../core/solver_messages.hpp"

namespace wta::net {

using namespace std::chrono;

struct ISolverClient {
    virtual ~ISolverClient() = default;
    
    // ==================== 新接口 ====================
    
    // 上报战场状态（fire-and-forget，不需要响应）
    virtual bool report_status(const wta::proto::StatusReportEvent& event,
                              milliseconds timeout = milliseconds(500)) = 0;
    
    // 上报实体击毁事件
    virtual bool report_killed(const wta::proto::EntityKilledEvent& event,
                              milliseconds timeout = milliseconds(200)) = 0;
    
    // 上报伤害事件
    virtual bool report_damage(const wta::proto::DamageEvent& event,
                              milliseconds timeout = milliseconds(200)) = 0;
    
    // 上报开火事件
    virtual bool report_fired(const wta::proto::FiredEvent& event,
                             milliseconds timeout = milliseconds(200)) = 0;
    
    // 发送日志消息（fire-and-forget，不需要响应）
    virtual bool send_log(const wta::proto::LogMessage& log_msg,
                         milliseconds timeout = milliseconds(100)) = 0;
    
    // 请求WTA规划
    virtual bool request_plan(const wta::proto::PlanRequest& req,
                             wta::proto::PlanResponse& out,
                             milliseconds timeout = milliseconds(1000)) = 0;
    
    // ==================== 旧接口（废弃） ====================
    
    [[deprecated("Use request_plan instead")]]
    virtual bool solve(const wta::proto::SolveRequest& req,
                       wta::proto::SolveResponse& out,
                       milliseconds timeout = milliseconds(1000)) = 0;
};

struct ZmqSolverClientOptions {
    std::string endpoint{"tcp://127.0.0.1:5555"};
    int request_timeout_ms{1000};
};

std::unique_ptr<ISolverClient> make_zmq_solver_client(const ZmqSolverClientOptions&);

} // namespace wta::net
