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

inline void from_proto(const wta::pb::PlanResponse& from, wta::proto::PlanResponse& to) {
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
    to.stats.computation_time = from.stats().computation_time();
    to.stats.iterations = from.stats().iterations();
    to.stats.is_valid = from.stats().is_valid();
    to.stats.coverage_rate = from.stats().coverage_rate();
    to.ttl_sec = from.ttl_sec();
    to.error_msg = from.error_msg();
}

// ==================== 序列化/反序列化辅助函数 ====================

inline std::string serialize_protobuf(const google::protobuf::Message& msg) {
    std::string output;
    msg.SerializeToString(&output);
    return output;
}

template<typename T>
inline bool deserialize_protobuf(const std::string& data, T& msg) {
    return msg.ParseFromString(data);
}

} // namespace wta::net
