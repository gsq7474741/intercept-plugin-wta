#pragma once
#include <string>
#include <vector>
#include "types.hpp"
#include "config.hpp"

/**
 * @file solver_messages.hpp
 * @brief C++消息包装器 - 便于在代码中使用
 * 
 * 注意：真正的消息定义在 proto/wta_messages.proto
 * 
 * 这个文件提供的C++结构体只是便利包装器，用于：
 * 1. 在C++代码中方便地构造消息
 * 2. 避免直接依赖protobuf生成的类型
 * 
 * 序列化/反序列化由 wta/net/protobuf_adapter.hpp 负责
 * 所有ZMQ通信统一使用Protobuf二进制格式
 */

namespace wta::proto {

// ==================== 消息类型 ====================
enum class MessageType {
    StatusReport,     // 战场状态上报
    EntityKilled,     // 单位击毁事件
    Damage,           // 伤害事件
    Fired,            // 开火事件
    PlanRequest,      // 请求WTA规划
    PlanResponse      // WTA规划响应
};

// ==================== 数据上报消息 ====================

// 定期战场状态上报
struct StatusReportEvent {
    std::string type{"status_report"};
    double timestamp{0.0};
    std::vector<wta::types::PlatformState> platforms;
    std::vector<wta::types::TargetState> targets;
};

// 单位击毁事件
struct EntityKilledEvent {
    std::string type{"entity_killed"};
    double timestamp{0.0};
    int entity_id{0};
    std::string entity_type;  // "platform" or "target"
    std::string killed_by;    // 击毁者
};

// 伤害事件
struct DamageEvent {
    std::string type{"damage"};
    double timestamp{0.0};
    int entity_id{0};
    std::string entity_type;
    float damage_amount{0.0f};
    std::string source;
};

// 开火事件
struct FiredEvent {
    std::string type{"fired"};
    double timestamp{0.0};
    int platform_id{0};
    int target_id{0};
    std::string weapon;
    int ammo_left{0};
};

// ==================== WTA规划消息 ====================

// WTA规划请求
struct PlanRequest {
    std::string type{"plan_request"};
    double timestamp{0.0};
    std::string reason;  // "replan", "ttl_expired", "manual"
    wta::config::SolveConfig config{};
    std::vector<wta::types::PlatformState> platforms;
    std::vector<wta::types::TargetState> targets;
};

// 规划统计
struct PlanStats {
    double computation_time{0.0};
    int iterations{0};
    bool is_valid{false};
    double coverage_rate{0.0};
};

// WTA规划响应
struct PlanResponse {
    std::string type{"plan_response"};
    std::string status;  // "ok", "error", "no_solution"
    double timestamp{0.0};
    double best_fitness{0.0};
    wta::types::AssignmentMatrix assignment;
    size_t n_platforms{0};
    size_t n_targets{0};
    PlanStats stats{};
    double ttl_sec{2.0};  // 规划有效期（秒）
    std::string error_msg;  // 错误信息（如果有）
};

// ==================== 兼容旧接口（废弃） ====================
// 保留用于向后兼容，新代码请使用上面的消息类型

struct [[deprecated("Use PlanRequest instead")]] SolveRequest {
    std::string mission_id;
    double timestamp{0.0};
    wta::config::SolveConfig config{};
    std::vector<wta::types::PlatformState> platforms;
    std::vector<wta::types::TargetState> targets;
};

struct [[deprecated("Use PlanStats instead")]] AssignmentDetails {
    bool is_valid{false};
    double coverage_rate{0.0};
};

struct [[deprecated("Use PlanStats instead")]] SolveStats {
    double computation_time{0.0};
    int iterations{0};
};

struct [[deprecated("Use PlanResponse instead")]] SolveResponse {
    std::string status;
    double best_fitness{0.0};
    wta::types::AssignmentMatrix assignment;
    size_t n_platforms{0};
    size_t n_targets{0};
    AssignmentDetails details{};
    SolveStats stats{};
    double ttl_sec{2.0};
};

} // namespace wta::proto
