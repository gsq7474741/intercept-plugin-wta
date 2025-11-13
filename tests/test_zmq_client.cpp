#include "../src/wta/net/solver_client.hpp"
#include <iostream>
int main(){
    wta::net::ZmqSolverClientOptions opts; 
    opts.endpoint = "tcp://127.0.0.1:5555";
    auto client = wta::net::make_zmq_solver_client(opts);
    wta::proto::SolveRequest req{};
    wta::proto::SolveResponse resp{};
    bool ok = client->solve(req, resp, std::chrono::milliseconds(10));
    std::cout << (ok?resp.status:"fail") << "\n";
    return 0;
}
