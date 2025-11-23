#pragma once
#include <glog/logging.h>
#include <memory>
#include <string>
#include <atomic>
#include <chrono>

namespace wta::net {

// 前向声明：必须与 solver_client.hpp 中保持一致（struct）
struct ISolverClient;

/**
 * @brief ZMQ 日志 Sink - 将 glog 日志通过 ZMQ 发送到 Dashboard
 * 
 * 使用方法:
 * 1. 创建 LogSinkZmq 实例
 * 2. 调用 enable() 启用
 * 3. 所有 LOG(INFO/WARNING/ERROR) 会自动发送到 Dashboard
 */
class LogSinkZmq : public google::LogSink {
public:
    /**
     * @brief 构造函数
     * @param solver_client ZMQ 客户端（用于发送日志）
     * @param component 组件名称（如 "Orchestrator", "WorldSampler"）
     */
    explicit LogSinkZmq(std::shared_ptr<ISolverClient> solver_client, const std::string& component = "WTA");
    
    ~LogSinkZmq() override;
    
    /**
     * @brief glog 回调 - 处理日志消息
     */
    void send(google::LogSeverity severity, const char* full_filename,
              const char* base_filename, int line,
              const struct ::tm* tm_time,
              const char* message, size_t message_len) override;
    
    /**
     * @brief 启用日志发送
     */
    void enable();
    
    /**
     * @brief 禁用日志发送
     */
    void disable();
    
    /**
     * @brief 是否启用
     */
    bool is_enabled() const { return enabled_; }
    
    /**
     * @brief 设置最小日志级别（低于此级别的日志不发送）
     * @param min_level 0=DEBUG, 1=INFO, 2=WARNING, 3=ERROR
     */
    void set_min_level(int min_level) { min_level_ = min_level; }
    
private:
    std::shared_ptr<ISolverClient> solver_client_;
    std::string component_;
    std::atomic<bool> enabled_{false};
    int min_level_{0};  // 默认发送所有日志
};

/**
 * @brief 全局日志 Sink 管理器
 */
class LogSinkManager {
public:
    static LogSinkManager& instance();
    
    /**
     * @brief 初始化日志 Sink
     */
    void initialize(std::shared_ptr<ISolverClient> solver_client, const std::string& component = "WTA");
    
    /**
     * @brief 启用日志发送
     */
    void enable();
    
    /**
     * @brief 禁用日志发送
     */
    void disable();
    
    /**
     * @brief 关闭日志 Sink
     */
    void shutdown();
    
private:
    LogSinkManager() = default;
    ~LogSinkManager();
    
    std::unique_ptr<LogSinkZmq> sink_;
};

} // namespace wta::net
