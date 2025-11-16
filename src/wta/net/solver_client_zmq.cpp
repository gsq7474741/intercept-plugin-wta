#include "solver_client.hpp"
#include "protobuf_adapter.hpp"
#include "wta_messages.pb.h"
#include <memory>
#include <string>
#include <sstream>
#include <chrono>
#include <cstring>
#include <ctime>
#include <intercept.hpp>

// 必须在命名空间外包含glog，避免符号命名空间污染
#ifdef WTA_HAVE_GLOG
#include <glog/logging.h>
#define WTA_LOG(level) LOG(level)
#else
#define WTA_LOG(level) if(false) std::cout
#endif

namespace wta::net {

using namespace std::chrono;

#ifdef WTA_HAVE_ZMQ
#include <zmq.h>

namespace {
struct ZmqCtx {
    void* ctx{nullptr};
    ZmqCtx() { ctx = zmq_ctx_new(); }
    ~ZmqCtx() { if (ctx) zmq_ctx_term(ctx); }
};

// 通用ZMQ发送函数（支持带响应和fire-and-forget）
bool send_zmq_message(const std::string& endpoint, const std::string& payload, 
                      std::string* response, milliseconds timeout) {
    ZmqCtx ctx;
    void* sock = zmq_socket(ctx.ctx, ZMQ_REQ);
    if (!sock) {
        WTA_LOG(ERROR) << "Failed to create ZMQ socket";
        return false;
    }
    
    int to = static_cast<int>(timeout.count());
    zmq_setsockopt(sock, ZMQ_RCVTIMEO, &to, sizeof(to));
    zmq_setsockopt(sock, ZMQ_SNDTIMEO, &to, sizeof(to));
    
    if (zmq_connect(sock, endpoint.c_str()) != 0) {
        WTA_LOG(ERROR) << "Failed to connect to " << endpoint;
        zmq_close(sock);
        return false;
    }
    
    zmq_msg_t zmsg;
    zmq_msg_init_size(&zmsg, payload.size());
    memcpy(zmq_msg_data(&zmsg), payload.data(), payload.size());
    
    if (zmq_msg_send(&zmsg, sock, 0) < 0) {
        WTA_LOG(ERROR) << "Failed to send ZMQ message";
        zmq_msg_close(&zmsg);
        zmq_close(sock);
        return false;
    }
    zmq_msg_close(&zmsg);
    
    // 如果需要响应
    if (response) {
        zmq_msg_t rep;
        zmq_msg_init(&rep);
        int rc = zmq_msg_recv(&rep, sock, 0);
        if (rc < 0) {
            WTA_LOG(ERROR) << "Failed to receive ZMQ response";
            zmq_msg_close(&rep);
            zmq_close(sock);
            return false;
        }
        *response = std::string((char*)zmq_msg_data(&rep), zmq_msg_size(&rep));
        zmq_msg_close(&rep);
        WTA_LOG(INFO) << "Received response: " << response->size() << " bytes";
    }
    
    zmq_close(sock);
    return true;
}

class ZmqSolverClient final : public ISolverClient {
public:
    explicit ZmqSolverClient(const ZmqSolverClientOptions& o) : opts_(o) {}
    
    // ==================== 新接口实现 ====================
    
    bool report_status(const wta::proto::StatusReportEvent& event, milliseconds timeout) override {
        WTA_LOG(INFO) << "Reporting status: " << event.platforms.size() 
                      << " platforms, " << event.targets.size() << " targets";
        std::string payload = wta::net::serialize_status_report(event);
        // fire-and-forget，不需要响应
        return send_zmq_message(opts_.endpoint, payload, nullptr, timeout);
    }
    
    bool report_killed(const wta::proto::EntityKilledEvent& event, milliseconds timeout) override {
        WTA_LOG(INFO) << "Reporting entity killed: " << event.entity_type 
                      << " #" << event.entity_id;
        std::string payload = wta::net::serialize_entity_killed(event);
        // fire-and-forget
        return send_zmq_message(opts_.endpoint, payload, nullptr, timeout);
    }
    
    bool report_damage(const wta::proto::DamageEvent& event, milliseconds timeout) override {
        WTA_LOG(INFO) << "Reporting damage: " << event.entity_type 
                      << " #" << event.entity_id << " damage=" << event.damage_amount;
        std::string payload = wta::net::serialize_damage(event);
        // fire-and-forget
        return send_zmq_message(opts_.endpoint, payload, nullptr, timeout);
    }
    
    bool report_fired(const wta::proto::FiredEvent& event, milliseconds timeout) override {
        WTA_LOG(INFO) << "Reporting fired: platform #" << event.platform_id 
                      << " -> target #" << event.target_id;
        std::string payload = wta::net::serialize_fired(event);
        // fire-and-forget
        return send_zmq_message(opts_.endpoint, payload, nullptr, timeout);
    }
    
    bool request_plan(const wta::proto::PlanRequest& req, wta::proto::PlanResponse& out, milliseconds timeout) override {
        WTA_LOG(INFO) << "Requesting plan: " << req.platforms.size() 
                      << " platforms, " << req.targets.size() << " targets, reason=" << req.reason;
        
        std::string payload = wta::net::serialize_plan_request(req);
        std::string response;
        
        if (!send_zmq_message(opts_.endpoint, payload, &response, timeout)) {
            WTA_LOG(ERROR) << "Failed to send plan request";
            return false;
        }
        
        // 反序列化响应
        if (!wta::net::deserialize_plan_response(response, out)) {
            WTA_LOG(ERROR) << "Failed to deserialize plan response";
            return false;
        }
        
        WTA_LOG(INFO) << "Received plan: status=" << out.status 
                      << ", fitness=" << out.best_fitness 
                      << ", assignments=" << out.assignment.size();
        return true;
    }
    
    // ==================== 旧接口实现 ====================
    
    // 旧接口实现（废弃，转发到request_plan）
    bool solve(const wta::proto::SolveRequest& req, wta::proto::SolveResponse& out, milliseconds timeout) override {
        WTA_LOG(WARNING) << "[DEPRECATED] solve() called, forwarding to request_plan()";
        intercept::sqf::diag_log("WTA: [DEPRECATED] solve() called");
        
        // 转换为PlanRequest
        wta::proto::PlanRequest plan_req;
        plan_req.timestamp = req.timestamp;
        plan_req.reason = "legacy_solve";
        plan_req.platforms = req.platforms;
        plan_req.targets = req.targets;
        
        wta::proto::PlanResponse plan_resp;
        bool ok = request_plan(plan_req, plan_resp, timeout);
        
        if (ok) {
            // 转换回旧格式
            out.status = plan_resp.status;
            out.best_fitness = plan_resp.best_fitness;
            out.assignment = plan_resp.assignment;
            out.n_platforms = plan_resp.n_platforms;
            out.n_targets = plan_resp.n_targets;
            out.details.is_valid = plan_resp.stats.is_valid;
            out.details.coverage_rate = plan_resp.stats.coverage_rate;
            out.stats.computation_time = plan_resp.stats.computation_time;
            out.stats.iterations = plan_resp.stats.iterations;
            out.ttl_sec = plan_resp.ttl_sec;
        }
        
        return ok;
    }
    
private:
    ZmqSolverClientOptions opts_;
};
}

std::unique_ptr<ISolverClient> make_zmq_solver_client(const ZmqSolverClientOptions& opts) {
    return std::make_unique<ZmqSolverClient>(opts);
}

#else

class ZmqSolverClientStub final : public ISolverClient {
public:
    explicit ZmqSolverClientStub(const ZmqSolverClientOptions&) {}
    bool report_status(const wta::proto::StatusReportEvent&, milliseconds) override { return false; }
    bool report_killed(const wta::proto::EntityKilledEvent&, milliseconds) override { return false; }
    bool report_damage(const wta::proto::DamageEvent&, milliseconds) override { return false; }
    bool report_fired(const wta::proto::FiredEvent&, milliseconds) override { return false; }
    bool request_plan(const wta::proto::PlanRequest&, wta::proto::PlanResponse&, milliseconds) override { return false; }
    bool solve(const wta::proto::SolveRequest&, wta::proto::SolveResponse&, milliseconds) override { return false; }
};

std::unique_ptr<ISolverClient> make_zmq_solver_client(const ZmqSolverClientOptions& opts) {
    return std::make_unique<ZmqSolverClientStub>(opts);
}

#endif

} // namespace wta::net
