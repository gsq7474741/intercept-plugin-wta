#include <intercept.hpp>
#include <thread>
#include <chrono>


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
}