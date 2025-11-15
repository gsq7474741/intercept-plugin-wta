#include "solver_client.hpp"
#include <memory>
#include <string>
#include <sstream>
#include <chrono>
#include <cstring>
#include <ctime>
#include <glog/logging.h>

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

// 序列化平台角色
const char* role_to_string(wta::types::PlatformRole role) {
    switch (role) {
        case wta::types::PlatformRole::AntiPersonnel: return "AntiPersonnel";
        case wta::types::PlatformRole::AntiArmor: return "AntiArmor";
        case wta::types::PlatformRole::MultiRole: return "MultiRole";
        default: return "Unknown";
    }
}

// 序列化目标类型
const char* kind_to_string(wta::types::TargetKind kind) {
    switch (kind) {
        case wta::types::TargetKind::Infantry: return "Infantry";
        case wta::types::TargetKind::Armor: return "Armor";
        case wta::types::TargetKind::SAM: return "SAM";
        case wta::types::TargetKind::Other: return "Other";
        default: return "Unknown";
    }
}

// 序列化SolveRequest为JSON
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
        oss << "\"role\":\"" << role_to_string(p.role) << "\",";
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
        oss << "\"kind\":\"" << kind_to_string(t.kind) << "\",";
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

class ZmqSolverClient final : public ISolverClient {
public:
    explicit ZmqSolverClient(const ZmqSolverClientOptions& o) : opts_(o) {}
    bool solve(const wta::proto::SolveRequest& req, wta::proto::SolveResponse& out, milliseconds timeout) override {
        LOG(INFO) << "Solving: " << req.platforms.size() << " platforms, " << req.targets.size() << " targets";
        
        ZmqCtx ctx;
        void* sock = zmq_socket(ctx.ctx, ZMQ_REQ);
        if (!sock) {
            LOG(ERROR) << "zmq_socket failed";
            return false;
        }
        
        int to = static_cast<int>(timeout.count());
        zmq_setsockopt(sock, ZMQ_RCVTIMEO, &to, sizeof(to));
        zmq_setsockopt(sock, ZMQ_SNDTIMEO, &to, sizeof(to));
        
        if (zmq_connect(sock, opts_.endpoint.c_str()) != 0) {
            LOG(ERROR) << "zmq_connect failed to " << opts_.endpoint;
            zmq_close(sock);
            return false;
        }
        
        std::string payload = serialize_request(req);
        LOG(INFO) << "Sending payload: " << payload.substr(0, 200) << "...";
        
        zmq_msg_t msg;
        zmq_msg_init_size(&msg, payload.size());
        memcpy(zmq_msg_data(&msg), payload.data(), payload.size());
        
        if (zmq_msg_send(&msg, sock, 0) < 0) {
            LOG(ERROR) << "zmq_msg_send failed";
            zmq_msg_close(&msg);
            zmq_close(sock);
            return false;
        }
        zmq_msg_close(&msg);
        
        zmq_msg_t rep;
        zmq_msg_init(&rep);
        int rc = zmq_msg_recv(&rep, sock, 0);
        
        if (rc < 0) {
            LOG(ERROR) << "zmq_msg_recv failed";
            zmq_msg_close(&rep);
            zmq_close(sock);
            return false;
        }
        
        out.status = "ok";
        out.best_fitness = 0.0;
        out.n_platforms = 0;
        out.n_targets = 0;
        out.ttl_sec = 1.0;
        
        LOG(INFO) << "Solve succeeded: fitness=" << out.best_fitness;
        
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
    bool solve(const wta::proto::SolveRequest&, wta::proto::SolveResponse&, milliseconds) override { return false; }
};

std::unique_ptr<ISolverClient> make_zmq_solver_client(const ZmqSolverClientOptions& opts) {
    return std::make_unique<ZmqSolverClientStub>(opts);
}

#endif

} // namespace wta::net
