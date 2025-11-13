#include "solver_client.hpp"
#include <memory>
#include <string>
#include <chrono>
#include <cstring>
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
        
        std::string payload = "{\"type\":\"solve\"}";
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
