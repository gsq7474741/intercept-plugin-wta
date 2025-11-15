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

// 序列化为简单JSON
std::string serialize_request(const wta::proto::SolveRequest& req) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"type\":\"solve\",";
    oss << "\"timestamp\":" << std::time(nullptr) << ",";
    
    // 平台数组
    oss << "\"platforms\":[";
    for (size_t i = 0; i < req.platforms.size(); ++i) {
        const auto& p = req.platforms[i];
        if (i > 0) oss << ",";
        oss << "{";
        oss << "\"id\":" << p.id << ",";
        
        // Role
        const char* role_str = "Unknown";
        if (p.role == wta::types::PlatformRole::AntiPersonnel) role_str = "AntiPersonnel";
        else if (p.role == wta::types::PlatformRole::AntiArmor) role_str = "AntiArmor";
        else if (p.role == wta::types::PlatformRole::MultiRole) role_str = "MultiRole";
        oss << "\"role\":\"" << role_str << "\",";
        
        oss << "\"pos\":{\"x\":" << p.pos.x << ",\"y\":" << p.pos.y << "},";
        oss << "\"alive\":" << (p.alive ? "true" : "false") << ",";
        oss << "\"hit_prob\":" << p.hit_prob << ",";
        oss << "\"cost\":" << p.cost << ",";
        oss << "\"max_range\":" << p.max_range << ",";
        oss << "\"max_targets\":" << p.max_targets << ",";
        oss << "\"quantity\":" << p.quantity << ",";
        oss << "\"ammo\":{";
        oss << "\"missile\":" << p.ammo.missile << ",";
        oss << "\"bomb\":" << p.ammo.bomb << ",";
        oss << "\"rocket\":" << p.ammo.rocket;
        oss << "},";
        oss << "\"target_types\":[";
        bool first = true;
        for (const auto& tt : p.target_types) {
            if (!first) oss << ",";
            oss << tt;
            first = false;
        }
        oss << "]";
        oss << "}";
    }
    oss << "],";
    
    // 目标数组
    oss << "\"targets\":[";
    for (size_t i = 0; i < req.targets.size(); ++i) {
        const auto& t = req.targets[i];
        if (i > 0) oss << ",";
        oss << "{";
        oss << "\"id\":" << t.id << ",";
        
        // Kind
        const char* kind_str = "Unknown";
        if (t.kind == wta::types::TargetKind::Infantry) kind_str = "Infantry";
        else if (t.kind == wta::types::TargetKind::Armor) kind_str = "Armor";
        else if (t.kind == wta::types::TargetKind::SAM) kind_str = "SAM";
        else if (t.kind == wta::types::TargetKind::Other) kind_str = "Other";
        oss << "\"kind\":\"" << kind_str << "\",";
        
        oss << "\"pos\":{\"x\":" << t.pos.x << ",\"y\":" << t.pos.y << "},";
        oss << "\"alive\":" << (t.alive ? "true" : "false") << ",";
        oss << "\"value\":" << t.value << ",";
        oss << "\"tier\":" << t.tier;
        oss << "}";
    }
    oss << "]";
    
    oss << "}";
    return oss.str();
}

// 使用 Protobuf 序列化 StatusReportEvent
std::string serialize_status_report(const wta::proto::StatusReportEvent& event) {
    // 创建 protobuf 消息 (使用 wta::pb 命名空间)
    wta::pb::StatusReportEvent pb_event;
    pb_event.set_timestamp(event.timestamp);
    
    // 转换平台
    for (const auto& platform : event.platforms) {
        auto* pb_platform = pb_event.add_platforms();
        to_proto(platform, pb_platform);
    }
    
    // 转换目标
    for (const auto& target : event.targets) {
        auto* pb_target = pb_event.add_targets();
        to_proto(target, pb_target);
    }
    
    // 包装到 WTAMessage
    wta::pb::WTAMessage msg;
    *msg.mutable_status_report() = pb_event;
    
    // 序列化为二进制
    std::string binary;
    msg.SerializeToString(&binary);
    return binary;
}

// 使用 Protobuf 序列化 PlanRequest
std::string serialize_plan_request(const wta::proto::PlanRequest& req) {
    // 创建 protobuf 消息
    wta::pb::PlanRequest pb_req;
    pb_req.set_timestamp(req.timestamp);
    pb_req.set_reason(req.reason);
    
    // 转换平台
    for (const auto& platform : req.platforms) {
        auto* pb_platform = pb_req.add_platforms();
        to_proto(platform, pb_platform);
    }
    
    // 转换目标
    for (const auto& target : req.targets) {
        auto* pb_target = pb_req.add_targets();
        to_proto(target, pb_target);
    }
    
    // 包装到 WTAMessage
    wta::pb::WTAMessage msg;
    *msg.mutable_plan_request() = pb_req;
    
    // 序列化为二进制
    std::string binary;
    msg.SerializeToString(&binary);
    return binary;
}

// 通用ZMQ发送函数
bool send_zmq_message(const std::string& endpoint, const std::string& payload, 
                      std::string* response, milliseconds timeout) {
    ZmqCtx ctx;
    void* sock = zmq_socket(ctx.ctx, ZMQ_REQ);
    if (!sock) return false;
    
    int to = static_cast<int>(timeout.count());
    zmq_setsockopt(sock, ZMQ_RCVTIMEO, &to, sizeof(to));
    zmq_setsockopt(sock, ZMQ_SNDTIMEO, &to, sizeof(to));
    
    if (zmq_connect(sock, endpoint.c_str()) != 0) {
        zmq_close(sock);
        return false;
    }
    
    zmq_msg_t zmsg;
    zmq_msg_init_size(&zmsg, payload.size());
    memcpy(zmq_msg_data(&zmsg), payload.data(), payload.size());
    
    if (zmq_msg_send(&zmsg, sock, 0) < 0) {
        zmq_msg_close(&zmsg);
        zmq_close(sock);
        return false;
    }
    zmq_msg_close(&zmsg);
    
    if (response) {
        zmq_msg_t rep;
        zmq_msg_init(&rep);
        int rc = zmq_msg_recv(&rep, sock, 0);
        if (rc < 0) {
            zmq_msg_close(&rep);
            zmq_close(sock);
            return false;
        }
        *response = std::string((char*)zmq_msg_data(&rep), zmq_msg_size(&rep));
        zmq_msg_close(&rep);
    }
    
    zmq_close(sock);
    return true;
}

class ZmqSolverClient final : public ISolverClient {
public:
    explicit ZmqSolverClient(const ZmqSolverClientOptions& o) : opts_(o) {}
    
    // ==================== 新接口实现 ====================
    
    bool report_status(const wta::proto::StatusReportEvent& event, milliseconds timeout) override {
        std::string payload = serialize_status_report(event);
        // fire-and-forget，不需要响应
        return send_zmq_message(opts_.endpoint, payload, nullptr, timeout);
    }
    
    bool report_killed(const wta::proto::EntityKilledEvent& event, milliseconds timeout) override {
        // 简单实现：将来可以优化
        return true;
    }
    
    bool report_damage(const wta::proto::DamageEvent& event, milliseconds timeout) override {
        return true;
    }
    
    bool report_fired(const wta::proto::FiredEvent& event, milliseconds timeout) override {
        return true;
    }
    
    bool request_plan(const wta::proto::PlanRequest& req, wta::proto::PlanResponse& out, milliseconds timeout) override {
        std::string payload = serialize_plan_request(req);
        std::string response;
        
        if (!send_zmq_message(opts_.endpoint, payload, &response, timeout)) {
            return false;
        }
        
        // 解析响应（简单实现）
        out.status = "ok";
        out.ttl_sec = 1.0;
        return true;
    }
    
    // ==================== 旧接口实现 ====================
    
    bool solve(const wta::proto::SolveRequest& req, wta::proto::SolveResponse& out, milliseconds timeout) override {
        char msg[256];
        sprintf_s(msg, sizeof(msg), "WTA: Sending %d platforms, %d targets to ZMQ", 
                  (int)req.platforms.size(), (int)req.targets.size());
        intercept::sqf::diag_log(msg);
        
        ZmqCtx ctx;
        void* sock = zmq_socket(ctx.ctx, ZMQ_REQ);
        if (!sock) {
            intercept::sqf::diag_log("WTA: zmq_socket failed");
            return false;
        }
        
        int to = static_cast<int>(timeout.count());
        zmq_setsockopt(sock, ZMQ_RCVTIMEO, &to, sizeof(to));
        zmq_setsockopt(sock, ZMQ_SNDTIMEO, &to, sizeof(to));
        
        if (zmq_connect(sock, opts_.endpoint.c_str()) != 0) {
            sprintf_s(msg, sizeof(msg), "WTA: zmq_connect failed to %s", opts_.endpoint.c_str());
            intercept::sqf::diag_log(msg);
            zmq_close(sock);
            return false;
        }
        
        std::string payload = serialize_request(req);
        sprintf_s(msg, sizeof(msg), "WTA: Payload size: %d bytes", (int)payload.size());
        intercept::sqf::diag_log(msg);
        
        zmq_msg_t zmsg;
        zmq_msg_init_size(&zmsg, payload.size());
        memcpy(zmq_msg_data(&zmsg), payload.data(), payload.size());
        
        if (zmq_msg_send(&zmsg, sock, 0) < 0) {
            intercept::sqf::diag_log("WTA: zmq_msg_send failed");
            zmq_msg_close(&zmsg);
            zmq_close(sock);
            return false;
        }
        zmq_msg_close(&zmsg);
        
        intercept::sqf::diag_log("WTA: Message sent, waiting for response...");
        
        zmq_msg_t rep;
        zmq_msg_init(&rep);
        int rc = zmq_msg_recv(&rep, sock, 0);
        
        if (rc < 0) {
            intercept::sqf::diag_log("WTA: zmq_msg_recv failed");
            zmq_msg_close(&rep);
            zmq_close(sock);
            return false;
        }
        
        intercept::sqf::diag_log("WTA: Response received!");
        
        out.status = "ok";
        out.best_fitness = 0.0;
        out.n_platforms = 0;
        out.n_targets = 0;
        out.ttl_sec = 1.0;
        
        zmq_msg_close(&rep);
        zmq_close(sock);
        return true;
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
