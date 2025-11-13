#include <gtest/gtest.h>
#include "../src/wta/world/event_bus.hpp"

class EventBusTest : public ::testing::Test {
protected:
    wta::events::EventBus bus;
};

TEST_F(EventBusTest, PublishAndPopEvent) {
    wta::events::Event e{wta::events::EventType::ReplanRequest, wta::events::EventPayload{}, 0.0};
    bus.publish(e);
    
    wta::events::Event out{};
    bool ok = bus.try_pop(out);
    EXPECT_TRUE(ok);
    EXPECT_EQ(out.type, wta::events::EventType::ReplanRequest);
}

TEST_F(EventBusTest, PopFromEmptyBus) {
    wta::events::Event out{};
    bool ok = bus.try_pop(out);
    EXPECT_FALSE(ok);
}

TEST_F(EventBusTest, MultipleEvents) {
    for (int i = 0; i < 5; ++i) {
        wta::events::Event e{wta::events::EventType::EntityKilled, 
                            wta::events::EntityKilledEvent{static_cast<int32_t>(i), false}, 0.0};
        bus.publish(e);
    }
    
    for (int i = 0; i < 5; ++i) {
        wta::events::Event out{};
        EXPECT_TRUE(bus.try_pop(out));
        EXPECT_EQ(out.type, wta::events::EventType::EntityKilled);
    }
    
    wta::events::Event out{};
    EXPECT_FALSE(bus.try_pop(out));
}
