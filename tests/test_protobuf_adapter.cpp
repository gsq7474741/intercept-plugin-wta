#include <gtest/gtest.h>
#include "../src/wta/net/protobuf_adapter.hpp"
#include "../src/wta/core/types.hpp"

using namespace wta::net;
using namespace wta::types;
using namespace wta::proto;

// ==================== 基础类型转换测试 ====================

TEST(ProtobufAdapter, Vec2Conversion) {
    // C++ -> Proto
    Vec2 cpp_vec{1.5f, 2.5f};
    wta::pb::Vec2 pb_vec;
    to_proto(cpp_vec, &pb_vec);
    
    EXPECT_FLOAT_EQ(pb_vec.x(), 1.5f);
    EXPECT_FLOAT_EQ(pb_vec.y(), 2.5f);
    
    // Proto -> C++
    Vec2 cpp_vec2;
    from_proto(pb_vec, cpp_vec2);
    EXPECT_FLOAT_EQ(cpp_vec2.x, 1.5f);
    EXPECT_FLOAT_EQ(cpp_vec2.y, 2.5f);
}

TEST(ProtobufAdapter, AmmoStateConversion) {
    AmmoState cpp_ammo{4, 2, 8};
    wta::pb::AmmoState pb_ammo;
    to_proto(cpp_ammo, &pb_ammo);
    
    EXPECT_EQ(pb_ammo.missile(), 4);
    EXPECT_EQ(pb_ammo.bomb(), 2);
    EXPECT_EQ(pb_ammo.rocket(), 8);
}

TEST(ProtobufAdapter, PlatformRoleConversion) {
    EXPECT_EQ(to_proto_role(PlatformRole::AntiPersonnel), 
              wta::pb::PLATFORM_ROLE_ANTI_PERSONNEL);
    EXPECT_EQ(to_proto_role(PlatformRole::AntiArmor), 
              wta::pb::PLATFORM_ROLE_ANTI_ARMOR);
    EXPECT_EQ(to_proto_role(PlatformRole::MultiRole), 
              wta::pb::PLATFORM_ROLE_MULTI_ROLE);
    
    EXPECT_EQ(from_proto_role(wta::pb::PLATFORM_ROLE_ANTI_PERSONNEL), 
              PlatformRole::AntiPersonnel);
    EXPECT_EQ(from_proto_role(wta::pb::PLATFORM_ROLE_ANTI_ARMOR), 
              PlatformRole::AntiArmor);
    EXPECT_EQ(from_proto_role(wta::pb::PLATFORM_ROLE_MULTI_ROLE), 
              PlatformRole::MultiRole);
}

TEST(ProtobufAdapter, TargetKindConversion) {
    EXPECT_EQ(to_proto_kind(TargetKind::Infantry), 
              wta::pb::TARGET_KIND_INFANTRY);
    EXPECT_EQ(to_proto_kind(TargetKind::Armor), 
              wta::pb::TARGET_KIND_ARMOR);
    EXPECT_EQ(to_proto_kind(TargetKind::SAM), 
              wta::pb::TARGET_KIND_SAM);
    
    EXPECT_EQ(from_proto_kind(wta::pb::TARGET_KIND_INFANTRY), 
              TargetKind::Infantry);
    EXPECT_EQ(from_proto_kind(wta::pb::TARGET_KIND_ARMOR), 
              TargetKind::Armor);
    EXPECT_EQ(from_proto_kind(wta::pb::TARGET_KIND_SAM), 
              TargetKind::SAM);
}

// ==================== 复杂类型转换测试 ====================

TEST(ProtobufAdapter, PlatformStateConversion) {
    PlatformState cpp_platform;
    cpp_platform.id = 1;
    cpp_platform.role = PlatformRole::AntiPersonnel;
    cpp_platform.pos = Vec2{100.0f, 200.0f};
    cpp_platform.alive = true;
    cpp_platform.hit_prob = 0.85f;
    cpp_platform.cost = 1000.0f;
    cpp_platform.max_range = 5000.0f;
    cpp_platform.max_targets = 3;
    cpp_platform.quantity = 1;
    cpp_platform.ammo = AmmoState{4, 2, 8};
    cpp_platform.target_types = {1, 2};
    
    wta::pb::PlatformState pb_platform;
    to_proto(cpp_platform, &pb_platform);
    
    EXPECT_EQ(pb_platform.id(), 1);
    EXPECT_EQ(pb_platform.role(), wta::pb::PLATFORM_ROLE_ANTI_PERSONNEL);
    EXPECT_FLOAT_EQ(pb_platform.pos().x(), 100.0f);
    EXPECT_FLOAT_EQ(pb_platform.pos().y(), 200.0f);
    EXPECT_TRUE(pb_platform.alive());
    EXPECT_FLOAT_EQ(pb_platform.hit_prob(), 0.85f);
    EXPECT_FLOAT_EQ(pb_platform.cost(), 1000.0f);
    EXPECT_FLOAT_EQ(pb_platform.max_range(), 5000.0f);
    EXPECT_EQ(pb_platform.max_targets(), 3);
    EXPECT_EQ(pb_platform.quantity(), 1);
    EXPECT_EQ(pb_platform.ammo().missile(), 4);
    EXPECT_EQ(pb_platform.ammo().bomb(), 2);
    EXPECT_EQ(pb_platform.ammo().rocket(), 8);
    EXPECT_EQ(pb_platform.target_types_size(), 2);
    EXPECT_EQ(pb_platform.target_types(0), 1);
    EXPECT_EQ(pb_platform.target_types(1), 2);
}

TEST(ProtobufAdapter, TargetStateConversion) {
    TargetState cpp_target;
    cpp_target.id = 10;
    cpp_target.kind = TargetKind::Infantry;
    cpp_target.pos = Vec2{500.0f, 600.0f};
    cpp_target.alive = true;
    cpp_target.value = 100.0f;
    cpp_target.tier = 1;
    
    wta::pb::TargetState pb_target;
    to_proto(cpp_target, &pb_target);
    
    EXPECT_EQ(pb_target.id(), 10);
    EXPECT_EQ(pb_target.kind(), wta::pb::TARGET_KIND_INFANTRY);
    EXPECT_FLOAT_EQ(pb_target.pos().x(), 500.0f);
    EXPECT_FLOAT_EQ(pb_target.pos().y(), 600.0f);
    EXPECT_TRUE(pb_target.alive());
    EXPECT_FLOAT_EQ(pb_target.value(), 100.0f);
    EXPECT_EQ(pb_target.tier(), 1);
}

// ==================== 消息序列化测试 ====================

TEST(ProtobufAdapter, StatusReportSerialization) {
    StatusReportEvent event;
    event.timestamp = 123456.789;
    
    PlatformState platform;
    platform.id = 1;
    platform.role = PlatformRole::MultiRole;
    platform.pos = Vec2{100.0f, 200.0f};
    platform.alive = true;
    event.platforms.push_back(platform);
    
    TargetState target;
    target.id = 10;
    target.kind = TargetKind::Armor;
    target.pos = Vec2{300.0f, 400.0f};
    target.alive = true;
    event.targets.push_back(target);
    
    // 序列化
    std::string binary = serialize_status_report(event);
    EXPECT_GT(binary.size(), 0);
    
    // 反序列化验证
    wta::pb::WTAMessage msg;
    EXPECT_TRUE(msg.ParseFromString(binary));
    EXPECT_TRUE(msg.has_status_report());
    
    const auto& pb_event = msg.status_report();
    EXPECT_DOUBLE_EQ(pb_event.timestamp(), 123456.789);
    EXPECT_EQ(pb_event.platforms_size(), 1);
    EXPECT_EQ(pb_event.targets_size(), 1);
    EXPECT_EQ(pb_event.platforms(0).id(), 1);
    EXPECT_EQ(pb_event.targets(0).id(), 10);
}

TEST(ProtobufAdapter, EntityKilledSerialization) {
    EntityKilledEvent event;
    event.timestamp = 123456.789;
    event.entity_id = 5;
    event.entity_type = "platform";
    event.killed_by = "enemy";
    
    std::string binary = serialize_entity_killed(event);
    EXPECT_GT(binary.size(), 0);
    
    wta::pb::WTAMessage msg;
    EXPECT_TRUE(msg.ParseFromString(binary));
    EXPECT_TRUE(msg.has_entity_killed());
    
    const auto& pb_event = msg.entity_killed();
    EXPECT_DOUBLE_EQ(pb_event.timestamp(), 123456.789);
    EXPECT_EQ(pb_event.entity_id(), 5);
    EXPECT_EQ(pb_event.entity_type(), "platform");
    EXPECT_EQ(pb_event.killed_by(), "enemy");
}

TEST(ProtobufAdapter, DamageSerialization) {
    DamageEvent event;
    event.timestamp = 123456.789;
    event.entity_id = 5;
    event.entity_type = "platform";
    event.damage_amount = 0.5f;
    event.source = "enemy_fire";
    
    std::string binary = serialize_damage(event);
    EXPECT_GT(binary.size(), 0);
    
    wta::pb::WTAMessage msg;
    EXPECT_TRUE(msg.ParseFromString(binary));
    EXPECT_TRUE(msg.has_damage());
    
    const auto& pb_event = msg.damage();
    EXPECT_DOUBLE_EQ(pb_event.timestamp(), 123456.789);
    EXPECT_EQ(pb_event.entity_id(), 5);
    EXPECT_EQ(pb_event.entity_type(), "platform");
    EXPECT_FLOAT_EQ(pb_event.damage_amount(), 0.5f);
    EXPECT_EQ(pb_event.source(), "enemy_fire");
}

TEST(ProtobufAdapter, FiredSerialization) {
    FiredEvent event;
    event.timestamp = 123456.789;
    event.platform_id = 1;
    event.target_id = 10;
    event.weapon = "missiles_SCALPEL";
    event.ammo_left = 3;
    
    std::string binary = serialize_fired(event);
    EXPECT_GT(binary.size(), 0);
    
    wta::pb::WTAMessage msg;
    EXPECT_TRUE(msg.ParseFromString(binary));
    EXPECT_TRUE(msg.has_fired());
    
    const auto& pb_event = msg.fired();
    EXPECT_DOUBLE_EQ(pb_event.timestamp(), 123456.789);
    EXPECT_EQ(pb_event.platform_id(), 1);
    EXPECT_EQ(pb_event.target_id(), 10);
    EXPECT_EQ(pb_event.weapon(), "missiles_SCALPEL");
    EXPECT_EQ(pb_event.ammo_left(), 3);
}

TEST(ProtobufAdapter, PlanRequestSerialization) {
    PlanRequest request;
    request.timestamp = 123456.789;
    request.reason = "event_triggered";
    
    PlatformState platform;
    platform.id = 1;
    platform.alive = true;
    request.platforms.push_back(platform);
    
    TargetState target;
    target.id = 10;
    target.alive = true;
    request.targets.push_back(target);
    
    std::string binary = serialize_plan_request(request);
    EXPECT_GT(binary.size(), 0);
    
    wta::pb::WTAMessage msg;
    EXPECT_TRUE(msg.ParseFromString(binary));
    EXPECT_TRUE(msg.has_plan_request());
    
    const auto& pb_req = msg.plan_request();
    EXPECT_DOUBLE_EQ(pb_req.timestamp(), 123456.789);
    EXPECT_EQ(pb_req.reason(), "event_triggered");
    EXPECT_EQ(pb_req.platforms_size(), 1);
    EXPECT_EQ(pb_req.targets_size(), 1);
}

TEST(ProtobufAdapter, PlanResponseDeserialization) {
    // 构造protobuf响应
    wta::pb::PlanResponse pb_resp;
    pb_resp.set_status("ok");
    pb_resp.set_timestamp(123456.789);
    pb_resp.set_best_fitness(0.95);
    pb_resp.set_n_platforms(2);
    pb_resp.set_n_targets(3);
    pb_resp.set_ttl_sec(2.0);
    
    (*pb_resp.mutable_assignment())[1] = 10;
    (*pb_resp.mutable_assignment())[2] = 11;
    
    auto* stats = pb_resp.mutable_stats();
    stats->set_computation_time(0.5);
    stats->set_iterations(100);
    stats->set_is_valid(true);
    stats->set_coverage_rate(1.0);
    
    wta::pb::WTAMessage msg;
    *msg.mutable_plan_response() = pb_resp;
    
    std::string binary = serialize_protobuf(msg);
    
    // 反序列化
    PlanResponse response;
    EXPECT_TRUE(deserialize_plan_response(binary, response));
    
    EXPECT_EQ(response.status, "ok");
    EXPECT_DOUBLE_EQ(response.timestamp, 123456.789);
    EXPECT_DOUBLE_EQ(response.best_fitness, 0.95);
    EXPECT_EQ(response.n_platforms, 2);
    EXPECT_EQ(response.n_targets, 3);
    EXPECT_DOUBLE_EQ(response.ttl_sec, 2.0);
    EXPECT_EQ(response.assignment.size(), 2);
    EXPECT_EQ(response.assignment[1], 10);
    EXPECT_EQ(response.assignment[2], 11);
    EXPECT_DOUBLE_EQ(response.stats.computation_time, 0.5);
    EXPECT_EQ(response.stats.iterations, 100);
    EXPECT_TRUE(response.stats.is_valid);
    EXPECT_DOUBLE_EQ(response.stats.coverage_rate, 1.0);
}

// ==================== 边界情况测试 ====================

TEST(ProtobufAdapter, EmptyMessagesSerialization) {
    // 空的StatusReport
    StatusReportEvent empty_status;
    empty_status.timestamp = 0.0;
    std::string binary = serialize_status_report(empty_status);
    EXPECT_GT(binary.size(), 0);
    
    wta::pb::WTAMessage msg;
    EXPECT_TRUE(msg.ParseFromString(binary));
    EXPECT_TRUE(msg.has_status_report());
    EXPECT_EQ(msg.status_report().platforms_size(), 0);
    EXPECT_EQ(msg.status_report().targets_size(), 0);
}

TEST(ProtobufAdapter, InvalidDeserialization) {
    // 无效的二进制数据
    std::string invalid_data = "invalid binary data";
    PlanResponse response;
    EXPECT_FALSE(deserialize_plan_response(invalid_data, response));
}

TEST(ProtobufAdapter, WrongMessageTypeDeserialization) {
    // 发送StatusReport，尝试反序列化为PlanResponse
    StatusReportEvent event;
    event.timestamp = 123456.789;
    std::string binary = serialize_status_report(event);
    
    PlanResponse response;
    EXPECT_FALSE(deserialize_plan_response(binary, response));
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
