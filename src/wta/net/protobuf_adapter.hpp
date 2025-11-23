#pragma once
#include "../core/solver_messages.hpp"
#include "wta_messages.pb.h"
#include <string>

namespace wta::net {

// ==================== 类型转换：C++ 原生类型 → Protobuf ====================
// 注意：protobuf 生成的类型在 wta::pb 命名空间

inline void to_proto(const wta::types::Vec2& from, wta::pb::Vec2* to) {
    to->set_x(from.x);
    to->set_y(from.y);
}

inline void to_proto(const wta::types::AmmoState& from, wta::pb::AmmoState* to) {
    to->set_missile(from.missile);
    to->set_bomb(from.bomb);
    to->set_rocket(from.rocket);
}

inline void to_proto(const wta::types::MagazineDetail& from, wta::pb::MagazineDetail* to) {
    to->set_name(from.name);
    to->set_ammo_count(from.ammo_count);
    to->set_loaded(from.loaded);
    to->set_type(from.type);
    to->set_location(from.location);
}

inline wta::pb::PlatformRole to_proto_role(wta::types::PlatformRole role) {
    switch (role) {
        case wta::types::PlatformRole::AntiPersonnel:
            return wta::pb::PLATFORM_ROLE_ANTI_PERSONNEL;
        case wta::types::PlatformRole::AntiArmor:
            return wta::pb::PLATFORM_ROLE_ANTI_ARMOR;
        case wta::types::PlatformRole::MultiRole:
            return wta::pb::PLATFORM_ROLE_MULTI_ROLE;
        default:
            return wta::pb::PLATFORM_ROLE_UNKNOWN;
    }
}

inline wta::pb::TargetKind to_proto_kind(wta::types::TargetKind kind) {
    switch (kind) {
        case wta::types::TargetKind::Infantry:
            return wta::pb::TARGET_KIND_INFANTRY;
        case wta::types::TargetKind::Armor:
            return wta::pb::TARGET_KIND_ARMOR;
        case wta::types::TargetKind::SAM:
            return wta::pb::TARGET_KIND_SAM;
        case wta::types::TargetKind::Other:
            return wta::pb::TARGET_KIND_OTHER;
        default:
            return wta::pb::TARGET_KIND_UNKNOWN;
    }
}

inline void to_proto(const wta::types::PlatformState& from, wta::pb::PlatformState* to) {
    to->set_id(from.id);
    to->set_role(to_proto_role(from.role));
    to_proto(from.pos, to->mutable_pos());
    to->set_alive(from.alive);
    to->set_hit_prob(from.hit_prob);
    to->set_cost(from.cost);
    to->set_max_range(from.max_range);
    to->set_max_targets(from.max_targets);
    to->set_quantity(from.quantity);
    to_proto(from.ammo, to->mutable_ammo());
    for (int tt : from.target_types) {
        to->add_target_types(tt);
    }
    to->set_platform_type(from.platform_type);
    
    // 弹夹详细信息
    for (const auto& mag : from.magazines) {
        auto* pb_mag = to->add_magazines();
        to_proto(mag, pb_mag);
    }
    
    // 油量和损伤
    to->set_fuel(from.fuel);
    to->set_damage(from.damage);
}

inline void to_proto(const wta::types::TargetState& from, wta::pb::TargetState* to) {
    to->set_id(from.id);
    to->set_kind(to_proto_kind(from.kind));
    to_proto(from.pos, to->mutable_pos());
    to->set_alive(from.alive);
    to->set_value(from.value);
    to->set_tier(from.tier);
}

// ==================== Protobuf → C++ 原生类型 ====================
// 用于反序列化 protobuf 响应

inline void from_proto(const wta::pb::Vec2& from, wta::types::Vec2& to) {
    to.x = from.x();
    to.y = from.y();
}

inline wta::types::PlatformRole from_proto_role(wta::pb::PlatformRole role) {
    switch (role) {
        case wta::pb::PLATFORM_ROLE_ANTI_PERSONNEL:
            return wta::types::PlatformRole::AntiPersonnel;
        case wta::pb::PLATFORM_ROLE_ANTI_ARMOR:
            return wta::types::PlatformRole::AntiArmor;
        case wta::pb::PLATFORM_ROLE_MULTI_ROLE:
            return wta::types::PlatformRole::MultiRole;
        default:
            return wta::types::PlatformRole::MultiRole;
    }
}

inline wta::types::TargetKind from_proto_kind(wta::pb::TargetKind kind) {
    switch (kind) {
        case wta::pb::TARGET_KIND_INFANTRY:
            return wta::types::TargetKind::Infantry;
        case wta::pb::TARGET_KIND_ARMOR:
            return wta::types::TargetKind::Armor;
        case wta::pb::TARGET_KIND_SAM:
            return wta::types::TargetKind::SAM;
        case wta::pb::TARGET_KIND_OTHER:
            return wta::types::TargetKind::Other;
        default:
            return wta::types::TargetKind::Other;
    }
}

// ==================== 消息类型转换：C++ → Protobuf ====================

inline void to_proto(const wta::proto::StatusReportEvent& from, wta::pb::StatusReportEvent* to) {
    to->set_timestamp(from.timestamp);
    for (const auto& platform : from.platforms) {
        auto* pb_platform = to->add_platforms();
        to_proto(platform, pb_platform);
    }
    for (const auto& target : from.targets) {
        auto* pb_target = to->add_targets();
        to_proto(target, pb_target);
    }
}

inline void to_proto(const wta::proto::EntityKilledEvent& from, wta::pb::EntityKilledEvent* to) {
    to->set_timestamp(from.timestamp);
    to->set_entity_id(from.entity_id);
    to->set_entity_type(from.entity_type);
    to->set_killed_by(from.killed_by);
}

inline void to_proto(const wta::proto::DamageEvent& from, wta::pb::DamageEvent* to) {
    to->set_timestamp(from.timestamp);
    to->set_entity_id(from.entity_id);
    to->set_entity_type(from.entity_type);
    to->set_damage_amount(from.damage_amount);
    to->set_source(from.source);
}

inline void to_proto(const wta::proto::FiredEvent& from, wta::pb::FiredEvent* to) {
    to->set_timestamp(from.timestamp);
    to->set_platform_id(from.platform_id);
    to->set_target_id(from.target_id);
    to->set_weapon(from.weapon);
    to->set_ammo_left(from.ammo_left);
}

inline void to_proto(const wta::proto::PlanRequest& from, wta::pb::PlanRequest* to) {
    to->set_timestamp(from.timestamp);
    to->set_reason(from.reason);
    for (const auto& platform : from.platforms) {
        auto* pb_platform = to->add_platforms();
        to_proto(platform, pb_platform);
    }
    for (const auto& target : from.targets) {
        auto* pb_target = to->add_targets();
        to_proto(target, pb_target);
    }
}

// ==================== Protobuf → C++ 原生类型 ====================

inline void from_proto(const wta::pb::PlanStats& from, wta::proto::PlanStats& to) {
    to.computation_time = from.computation_time();
    to.iterations = from.iterations();
    to.is_valid = from.is_valid();
    to.coverage_rate = from.coverage_rate();
}

inline void from_proto(const wta::pb::PlanResponse& from, wta::proto::PlanResponse& to) {
    to.type = "plan_response";
    to.status = from.status();
    to.timestamp = from.timestamp();
    to.best_fitness = from.best_fitness();
    
    // 转换 assignment map
    to.assignment.clear();
    for (const auto& pair : from.assignment()) {
        to.assignment[pair.first] = pair.second;
    }
    
    to.n_platforms = from.n_platforms();
    to.n_targets = from.n_targets();
    from_proto(from.stats(), to.stats);
    to.ttl_sec = from.ttl_sec();
    to.error_msg = from.error_msg();
}

// ==================== 序列化/反序列化辅助函数 ====================

// 序列化任意protobuf消息到二进制字符串
inline std::string serialize_protobuf(const google::protobuf::Message& msg) {
    std::string output;
    msg.SerializeToString(&output);
    return output;
}

// 从二进制字符串反序列化protobuf消息
template<typename T>
inline bool deserialize_protobuf(const std::string& data, T& msg) {
    return msg.ParseFromString(data);
}

// ==================== WTAMessage包装器序列化 ====================

// 将StatusReportEvent序列化为WTAMessage
inline std::string serialize_status_report(const wta::proto::StatusReportEvent& event) {
    wta::pb::StatusReportEvent pb_event;
    to_proto(event, &pb_event);
    
    wta::pb::WTAMessage msg;
    *msg.mutable_status_report() = pb_event;
    return serialize_protobuf(msg);
}

// 将EntityKilledEvent序列化为WTAMessage
inline std::string serialize_entity_killed(const wta::proto::EntityKilledEvent& event) {
    wta::pb::EntityKilledEvent pb_event;
    to_proto(event, &pb_event);
    
    wta::pb::WTAMessage msg;
    *msg.mutable_entity_killed() = pb_event;
    return serialize_protobuf(msg);
}

// 将DamageEvent序列化为WTAMessage
inline std::string serialize_damage(const wta::proto::DamageEvent& event) {
    wta::pb::DamageEvent pb_event;
    to_proto(event, &pb_event);
    
    wta::pb::WTAMessage msg;
    *msg.mutable_damage() = pb_event;
    return serialize_protobuf(msg);
}

// 将FiredEvent序列化为WTAMessage
inline std::string serialize_fired(const wta::proto::FiredEvent& event) {
    wta::pb::FiredEvent pb_event;
    to_proto(event, &pb_event);
    
    wta::pb::WTAMessage msg;
    *msg.mutable_fired() = pb_event;
    return serialize_protobuf(msg);
}

// 将PlanRequest序列化为WTAMessage
inline std::string serialize_plan_request(const wta::proto::PlanRequest& request) {
    wta::pb::PlanRequest pb_req;
    to_proto(request, &pb_req);
    
    wta::pb::WTAMessage msg;
    *msg.mutable_plan_request() = pb_req;
    return serialize_protobuf(msg);
}

// 从WTAMessage反序列化PlanResponse
inline bool deserialize_plan_response(const std::string& data, wta::proto::PlanResponse& response) {
    wta::pb::WTAMessage msg;
    if (!deserialize_protobuf(data, msg)) {
        return false;
    }
    
    if (!msg.has_plan_response()) {
        return false;
    }
    
    from_proto(msg.plan_response(), response);
    return true;
}

// 将LogMessage序列化为WTAMessage
inline std::string serialize_log(const wta::proto::LogMessage& log_msg) {
    wta::pb::LogMessage pb_log;
    pb_log.set_timestamp(log_msg.timestamp);
    pb_log.set_level(static_cast<wta::pb::LogLevel>(log_msg.level));
    pb_log.set_file(log_msg.file);
    pb_log.set_line(log_msg.line);
    pb_log.set_function(log_msg.function);
    pb_log.set_message(log_msg.message);
    pb_log.set_thread_id(log_msg.thread_id);
    pb_log.set_component(log_msg.component);
    
    wta::pb::WTAMessage msg;
    *msg.mutable_log() = pb_log;
    return serialize_protobuf(msg);
}

} // namespace wta::net
