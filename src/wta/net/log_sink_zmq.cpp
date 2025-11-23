#include "log_sink_zmq.hpp"
#include "solver_client.hpp"
#include "protobuf_adapter.hpp"
#include "../core/solver_messages.hpp"
#include <chrono>
#include <sstream>
#include <thread>

namespace wta::net {

LogSinkZmq::LogSinkZmq(std::shared_ptr<ISolverClient> solver_client, const std::string& component)
    : solver_client_(solver_client), component_(component) {
}

LogSinkZmq::~LogSinkZmq() {
    disable();
}

void LogSinkZmq::send(google::LogSeverity severity, const char* full_filename,
                     const char* base_filename, int line,
                     const struct ::tm* tm_time,
                     const char* message, size_t message_len) {
    // 检查是否启用
    if (!enabled_ || !solver_client_) {
        return;
    }
    
    // 检查日志级别
    if (severity < min_level_) {
        return;
    }
    
    try {
        // 创建日志消息（使用 C++ 包装器）
        wta::proto::LogMessage log_msg;
        
        // 时间戳
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        log_msg.timestamp = std::chrono::duration<double>(duration).count();
        
        // 日志级别映射
        switch (severity) {
            case google::GLOG_INFO:
                log_msg.level = wta::proto::LogLevel::Info;
                break;
            case google::GLOG_WARNING:
                log_msg.level = wta::proto::LogLevel::Warning;
                break;
            case google::GLOG_ERROR:
                log_msg.level = wta::proto::LogLevel::Error;
                break;
            case google::GLOG_FATAL:
                log_msg.level = wta::proto::LogLevel::Fatal;
                break;
            default:
                log_msg.level = wta::proto::LogLevel::Debug;
                break;
        }
        
        // 文件信息
        log_msg.file = base_filename ? base_filename : "";
        log_msg.line = line;
        log_msg.function = "";  // glog 的 send() 回调没有提供函数名，设为空
        
        // 消息内容
        std::string msg_str(message, message_len);
        // 移除末尾的换行符
        if (!msg_str.empty() && msg_str.back() == '\n') {
            msg_str.pop_back();
        }
        log_msg.message = msg_str;
        
        // 线程 ID
        std::ostringstream oss;
        oss << std::this_thread::get_id();
        try {
            log_msg.thread_id = std::hash<std::string>{}(oss.str());
        } catch (...) {
            log_msg.thread_id = 0;
        }
        
        // 组件名称
        log_msg.component = component_;
        
        // 通过 ZMQ 发送（fire-and-forget）
        solver_client_->send_log(log_msg, std::chrono::milliseconds(100));
        
    } catch (const std::exception& e) {
        // 日志发送失败，忽略（避免无限递归）
    }
}

void LogSinkZmq::enable() {
    if (!enabled_) {
        enabled_ = true;
        google::AddLogSink(this);
        LOG(INFO) << "LogSinkZmq enabled for component: " << component_;
    }
}

void LogSinkZmq::disable() {
    if (enabled_) {
        google::RemoveLogSink(this);
        enabled_ = false;
        LOG(INFO) << "LogSinkZmq disabled for component: " << component_;
    }
}

// ==================== LogSinkManager ====================

LogSinkManager& LogSinkManager::instance() {
    static LogSinkManager instance;
    return instance;
}

LogSinkManager::~LogSinkManager() {
    shutdown();
}

void LogSinkManager::initialize(std::shared_ptr<ISolverClient> solver_client, const std::string& component) {
    if (!sink_) {
        sink_ = std::make_unique<LogSinkZmq>(solver_client, component);
    }
}

void LogSinkManager::enable() {
    if (sink_) {
        sink_->enable();
    }
}

void LogSinkManager::disable() {
    if (sink_) {
        sink_->disable();
    }
}

void LogSinkManager::shutdown() {
    if (sink_) {
        sink_->disable();
        sink_.reset();
    }
}

} // namespace wta::net
