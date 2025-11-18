#pragma once
#include "LoggerConfig.h"
#include <memory>
#include <string>
#include <vector>

namespace logger {

// 前向声明
class LoggerCore;

/**
 * @brief 日志系统统一入口（Facade模式 + PIMPL）
 * 
 * 用户只需包含此头文件即可使用所有功能
 */
class Logger {
public:
    static Logger& instance();
    
    // ===== 初始化接口 =====
    
    /**
     * @brief 自动查找配置文件初始化
     * 查找顺序：./logger_config.json → /etc/logger_config.json → 默认配置
     */
    void init();
    
    /**
     * @brief 从指定配置文件初始化
     */
    void init(const std::string& config_path);
    
    /**
     * @brief 从配置对象初始化
     */
    void init(const LoggerConfig& config);
    
    // ===== 日志接口 =====
    
    void debug(const std::string& msg, const char* file, const char* func, int line);
    void info(const std::string& msg, const char* file, const char* func, int line);
    void warning(const std::string& msg, const char* file, const char* func, int line);
    void error(const std::string& msg, const char* file, const char* func, int line);
    void critical(const std::string& msg, const char* file, const char* func, int line);
    
    // ===== 特殊日志接口 =====
    
    void binary(const void* data, size_t size, const std::string& tag = "");
    void message(const std::string& topic, const std::string& type, 
                const std::vector<uint8_t>& data);
    
    // ===== 运行时控制 =====
    
    void setLevel(LogLevel level);
    void setAsync(bool enable);
    void reload();  // 重新加载配置文件
    void flush();   // 刷新所有缓冲
    
    // ===== 查询接口 =====
    
    LoggerConfig getConfig() const;
    bool isInitialized() const;
    
private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace logger