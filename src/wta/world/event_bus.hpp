#pragma once
#include <mutex>
#include <deque>
#include <condition_variable>
#include <variant>
#include <string>
#include "../core/types.hpp"

namespace wta::events {

enum class EventType : uint8_t {
    MissionEachFrame,
    EntityKilled,
    HandleDamage,
    Fired,
    ReplanRequest
};

struct EntityKilledEvent { wta::types::Id entity_id{0}; bool is_platform{false}; };
struct DamageEvent { wta::types::Id entity_id{0}; float damage{0.f}; bool is_platform{false}; };
struct FiredEvent { wta::types::PlatformId platform_id{0}; std::string weapon; };

using EventPayload = std::variant<EntityKilledEvent, DamageEvent, FiredEvent>;

struct Event {
    EventType type;
    EventPayload payload;
    double timestamp{0.0};
};

class EventBus {
public:
    void publish(Event e) {
        {
            std::lock_guard<std::mutex> lk(m_);
            q_.push_back(std::move(e));
        }
        cv_.notify_one();
    }
    bool try_pop(Event& out) {
        std::lock_guard<std::mutex> lk(m_);
        if (q_.empty()) return false;
        out = std::move(q_.front());
        q_.pop_front();
        return true;
    }
    Event wait_and_pop() {
        std::unique_lock<std::mutex> lk(m_);
        cv_.wait(lk, [&]{ return !q_.empty(); });
        Event e = std::move(q_.front());
        q_.pop_front();
        return e;
    }
private:
    std::mutex m_;
    std::deque<Event> q_;
    std::condition_variable cv_;
};

} // namespace wta::events
