#pragma once
#include "Logger.h"

// ===== 基础日志宏 =====
#define LOG_DEBUG(msg)    logger::Logger::instance().debug(msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_INFO(msg)     logger::Logger::instance().info(msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_WARN(msg)     logger::Logger::instance().warning(msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_ERROR(msg)    logger::Logger::instance().error(msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_CRITICAL(msg) logger::Logger::instance().critical(msg, __FILE__, __FUNCTION__, __LINE__)

// ===== 格式化日志宏 =====
#define LOG_FMT(level, fmt, ...) do { \
    char buf_[2048]; \
    snprintf(buf_, sizeof(buf_), fmt, ##__VA_ARGS__); \
    LOG_##level(std::string(buf_)); \
} while(0)

// 便捷别名
#define LOG_DEBUG_FMT(fmt, ...) LOG_FMT(DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO_FMT(fmt, ...)  LOG_FMT(INFO, fmt, ##__VA_ARGS__)
#define LOG_WARN_FMT(fmt, ...)  LOG_FMT(WARN, fmt, ##__VA_ARGS__)
#define LOG_ERROR_FMT(fmt, ...) LOG_FMT(ERROR, fmt, ##__VA_ARGS__)

// ===== 条件日志宏 =====
#define LOG_IF(level, condition, msg) do { \
    if (condition) { LOG_##level(msg); } \
} while(0)

// ===== 性能日志宏（带耗时统计）=====
#define LOG_SCOPE_TIME(name) \
    ScopeTimer __scope_timer__##__LINE__(name)

class ScopeTimer {
public:
    ScopeTimer(const std::string& name) 
        : name_(name), start_(std::chrono::high_resolution_clock::now()) {}
    
    ~ScopeTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start_);
        LOG_INFO_FMT("[PERF] %s took %ld ms", name_.c_str(), duration.count());
    }
    
private:
    std::string name_;
    std::chrono::high_resolution_clock::time_point start_;
};

// ===== Assert日志宏 =====
#define LOG_ASSERT(condition, msg) do { \
    if (!(condition)) { \
        LOG_CRITICAL("Assertion failed: " msg); \
        std::abort(); \
    } \
} while(0)