#include <gtest/gtest.h>
#include "rpf/event_bus.h"

class EventBusTest : public ::testing::Test {
protected:
    rpf::EventBus bus;
};

TEST_F(EventBusTest, SubscribeAndPublish) {
    int call_count = 0;
    bus.subscribe("test.event", [&](const std::string& event, const nlohmann::json& data) {
        call_count++;
        EXPECT_EQ(event, "test.event");
    });

    bus.publish("test.event");
    EXPECT_EQ(call_count, 1);
}

TEST_F(EventBusTest, MultipleSubscribers) {
    int count1 = 0, count2 = 0;

    bus.subscribe("test.event", [&](const std::string&, const nlohmann::json&) { count1++; });
    bus.subscribe("test.event", [&](const std::string&, const nlohmann::json&) { count2++; });

    bus.publish("test.event");
    EXPECT_EQ(count1, 1);
    EXPECT_EQ(count2, 1);
}

TEST_F(EventBusTest, Unsubscribe) {
    int call_count = 0;
    auto id = bus.subscribe("test.event", [&](const std::string&, const nlohmann::json&) {
        call_count++;
    });

    bus.publish("test.event");
    EXPECT_EQ(call_count, 1);

    bus.unsubscribe(id);
    bus.publish("test.event");
    EXPECT_EQ(call_count, 1);  // 不应该再次调用
}

TEST_F(EventBusTest, DataTransmission) {
    nlohmann::json received_data;
    bus.subscribe("test.event", [&](const std::string&, const nlohmann::json& data) {
        received_data = data;
    });

    nlohmann::json sent_data = {{"key", "value"}, {"number", 42}};
    bus.publish("test.event", sent_data);

    EXPECT_EQ(received_data["key"], "value");
    EXPECT_EQ(received_data["number"], 42);
}

TEST_F(EventBusTest, DifferentEvents) {
    int event1_count = 0, event2_count = 0;

    bus.subscribe("event.1", [&](const std::string&, const nlohmann::json&) { event1_count++; });
    bus.subscribe("event.2", [&](const std::string&, const nlohmann::json&) { event2_count++; });

    bus.publish("event.1");
    EXPECT_EQ(event1_count, 1);
    EXPECT_EQ(event2_count, 0);

    bus.publish("event.2");
    EXPECT_EQ(event1_count, 1);
    EXPECT_EQ(event2_count, 1);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
