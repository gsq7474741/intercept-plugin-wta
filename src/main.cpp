#include <intercept.hpp>
#include <thread>
#include <chrono>
#include <memory>
#include <glog/logging.h>
#include "wta/world/world_sampler.hpp"
#include "wta/exec/executor.hpp"
#include "wta/exec/sequential_attack_test.hpp"
#include "wta/orchestrator/orchestrator.hpp"
#include "wta/net/solver_client.hpp"
#include "wta/net/log_sink_zmq.hpp"
#include "wta/world/event_bus.hpp"


int intercept::api_version() { //This is required for the plugin to work.
    return INTERCEPT_SDK_API_VERSION;
}

void intercept::register_interfaces() {
    
}

void intercept::pre_start() {
    
}

void intercept::pre_init() {
    intercept::sqf::system_chat("The Intercept template plugin is running!");
}

using namespace intercept;

namespace {
// 测试开关：true 时启用顺序打击测试模式（不启动 Orchestrator / ZMQ 规划）
// false 时运行正常的 Orchestrator + ZMQ 工作流
constexpr bool kEnableSequentialAttackTest = true;
}

static std::unique_ptr<wta::net::ISolverClient> g_solver_client;
static std::unique_ptr<wta::world::IWorldSampler> g_sampler;
static std::unique_ptr<wta::exec::IExecutor> g_executor;
static std::unique_ptr<wta::orch::Orchestrator> g_orchestrator;
static wta::events::EventBus g_event_bus;

// 注意：不使用 on_frame() 回调，因为在该上下文中 sqf::get_pos() 可能返回缓存值
// 采样在 Reporter 线程中进行，使用 invoker_lock 保证线程安全

// Our custom function, instructing our follower unit to move to a location near the player every second
void follow_player(object follower) {
    while (true) {
        //New scope to allow the thread_lock to lock and release each cycle
        {
            // Blocks the renderer to allow the SQF engine to execute
            client::invoker_lock thread_lock;

            // Get the player object and store it
            auto player = sqf::player();

            // Get the position of the player, returned as a Vector3 in PositionAGLS format
            auto player_pos = sqf::get_pos(player);

            // Calculate a new position near the player for our follower to move to
            vector3 target_pos(player_pos.x + 50, player_pos.y + 30, player_pos.z);

            // Move our follower
            sqf::move_to(follower, target_pos);

        } // thread_lock leaves scope and releases its lock

        // Wait one second
        //Sleep(1000);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

// This function is exported and is called by the host at the end of mission initialization.
void intercept::post_init() {
    google::InitGoogleLogging("wtaPlugin");
    LOG(INFO) << "WTA plugin post_init starting...";

    // ===== 顺序打击测试模式（仅用于验证 C++ 插件控制能力） =====
    if (kEnableSequentialAttackTest) {
        sqf::system_chat("WTA TEST: Sequential attack test mode enabled");
        sqf::diag_log("WTA TEST: Starting sequential attack test (no Orchestrator / ZMQ)");
        wta::test::start_sequential_attack_test();
        return;
    }

    // Get the player object and store it
    auto player = sqf::player();
    // Get the position of the player, returned as a Vector3 in PositionAGLS format
    auto player_pos = sqf::get_pos(player);

    // Calculate a new position near the player for our follower to spawn at
    vector3 follower_start_pos(player_pos.x + 5, player_pos.y + 3, player_pos.z);

    // Make a new group so that we can make a new unit
    auto follower_group = sqf::create_group(sqf::east());

    // Make our new unit
    auto follower_unit = sqf::create_unit(follower_group, "O_G_Soldier_F", follower_start_pos);

    // Stop our new friend from shooting at us or cowering in fear
    sqf::disable_ai(follower_unit, sqf::ai_behaviour_types::TARGET);
    sqf::disable_ai(follower_unit, sqf::ai_behaviour_types::AUTOTARGET);
    sqf::disable_ai(follower_unit, sqf::ai_behaviour_types::AUTOCOMBAT);
    sqf::disable_ai(follower_unit, sqf::ai_behaviour_types::CHECKVISIBLE);

    // The doStop SQF function allows him to move independently from his group
    sqf::do_stop(follower_unit);

    // Make a new thread
    std::thread follower_thread(follow_player, follower_unit);

    // Allow that thread to execute independent of the Arma 3 thread
    follower_thread.detach();

    // Bootstrap WTA orchestrator
    // LOG(INFO) << "Initializing WTA orchestrator...";
    sqf::system_chat("WTA: Initializing orchestrator...");
    sqf::diag_log("WTA: Initializing orchestrator...");
    wta::net::ZmqSolverClientOptions zmq_opts{};
    zmq_opts.endpoint = "tcp://127.0.0.1:5555";
    g_solver_client = wta::net::make_zmq_solver_client(zmq_opts);
    
    // 启用日志流传输到 Dashboard
    auto solver_client_ptr = std::shared_ptr<wta::net::ISolverClient>(g_solver_client.get(), [](auto*){});
    wta::net::LogSinkManager::instance().initialize(solver_client_ptr, "WTA-Plugin");
    wta::net::LogSinkManager::instance().enable();
    LOG(INFO) << "Log streaming enabled";
    
    g_sampler = wta::world::make_intercept_world_sampler();
    g_executor = wta::exec::make_intercept_executor();
    g_orchestrator = std::make_unique<wta::orch::Orchestrator>(g_event_bus, *g_solver_client, *g_sampler, *g_executor);
    g_orchestrator->start();
    // LOG(INFO) << "WTA plugin initialized successfully";
    sqf::system_chat("WTA: Plugin initialized successfully!");
    sqf::diag_log("WTA: Plugin initialized successfully!");
}