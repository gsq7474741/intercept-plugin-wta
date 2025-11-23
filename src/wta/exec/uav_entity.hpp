#pragma once
#include <string>
#include <optional>
#include <memory>
#include "../core/types.hpp"

// 前向声明 intercept 类型
namespace intercept::types {
    class object;
}

namespace wta::exec {

/**
 * @brief UAV 运行状态
 */
enum class UavStatus : uint8_t {
    Idle,           // 空闲
    NavigatingToTarget,  // 导航到目标
    Aiming,         // 瞄准目标
    Firing,         // 开火
    Egressing,      // 撤离
    RTB,            // 返回基地
    Dead            // 已摧毁
};

/**
 * @brief UAV 实体，维护单个无人机的状态和游戏对象引用
 */
class UavEntity {
public:
    UavEntity(wta::types::PlatformId id, const intercept::types::object& game_obj);

    // 基本信息
    wta::types::PlatformId id() const { return id_; }
    const intercept::types::object& game_object() const;
    
    // 状态管理
    UavStatus status() const { return status_; }
    void set_status(UavStatus s) { status_ = s; }
    
    // 位置信息
    wta::types::Vec2 position() const;
    float altitude() const;
    
    // 状态查询
    bool is_alive() const;
    bool is_idle() const { return status_ == UavStatus::Idle; }
    bool is_busy() const { return status_ != UavStatus::Idle && status_ != UavStatus::RTB && status_ != UavStatus::Dead; }
    bool has_ammo() const;
    float fuel_level() const;
    float damage_level() const;
    
    // 任务相关
    void set_current_task(wta::types::TargetId tid) { current_target_ = tid; }
    std::optional<wta::types::TargetId> current_task() const { return current_target_; }
    void clear_task() { current_target_ = std::nullopt; }
    
    // 能力查询
    bool can_engage_target(const wta::types::Vec2& target_pos, float max_range) const;
    std::string get_best_weapon() const;
    
private:
    wta::types::PlatformId id_;
    std::shared_ptr<intercept::types::object> game_obj_;
    UavStatus status_{UavStatus::Idle};
    std::optional<wta::types::TargetId> current_target_;
};

/**
 * @brief 目标实体，维护单个目标的状态
 */
class TargetEntity {
public:
    TargetEntity(wta::types::TargetId id, const intercept::types::object& game_obj);

    wta::types::TargetId id() const { return id_; }
    const intercept::types::object& game_object() const;
    
    wta::types::Vec2 position() const;
    bool is_alive() const;
    float threat_level() const { return threat_level_; }
    void set_threat_level(float level) { threat_level_ = level; }
    
private:
    wta::types::TargetId id_;
    std::shared_ptr<intercept::types::object> game_obj_;
    float threat_level_{1.0f};
};

} // namespace wta::exec
