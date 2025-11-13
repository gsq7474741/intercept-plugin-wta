#pragma once
#include <chrono>
#include <memory>
#include <string>
#include "../core/solver_messages.hpp"

namespace wta::net {

using namespace std::chrono;

struct ISolverClient {
    virtual ~ISolverClient() = default;
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
