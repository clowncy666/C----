#pragma once
#include "ILogSink.h"
#include <memory>
#include <map>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <filesystem>
#include "../../include/logger/LoggerConfig.h"
#include <queue>
class LoggerCore;



class ILogEntry {
public:
    virtual ~ILogEntry() = default;
    // 纯虚函数：每种条目自己决定如何写入
    virtual void writeTo(const std::map<std::string, std::shared_ptr<ILogSink>>& sinks) = 0; 
    // 估算大小（用于磁盘空间检查）
    virtual size_t estimateSize() const { return 0; }
};
class TextLogEntry : public ILogEntry {
public:
    LogLevel level;
    std::string message;
    std::string file;
    std::string function;
    std::string timestamp;
    int line;


    TextLogEntry(LogLevel level, std::string message, std::string file, 
                 std::string function, std::string timestamp, int line)
        : level(level), message(std::move(message)), file(std::move(file)), 
          function(std::move(function)), timestamp(std::move(timestamp)), line(line) {}

    void writeTo(const std::map<std::string, std::shared_ptr<ILogSink>>& sinks) override;
    size_t estimateSize() const override { return message.size() + 128; }
};
class BinaryLogEntry : public ILogEntry {
public:
    std::vector<uint8_t> data;
    std::string tag;
    uint64_t timestamp;
    // 默认构造函数
    
    BinaryLogEntry(const std::vector<uint8_t>& data,const std::string& tag, uint64_t timestamp):
    data(data), tag(tag), timestamp(timestamp) {}
    void writeTo(const std::map<std::string, std::shared_ptr<ILogSink>>& sinks) override;
    size_t estimateSize() const override { return data.size() + tag.size() + 16; }
};


class MessageLogEntry : public ILogEntry {
public:
    std::string topic;
    std::string type;
    std::vector<uint8_t> data;
    uint64_t timestamp;
    MessageLogEntry(const std::string& topic, const std::string& type,
             const std::vector<uint8_t>& data, uint64_t timestamp)
    : topic(topic), type(type), data(data), timestamp(timestamp) {}

    void writeTo(const std::map<std::string, std::shared_ptr<ILogSink>>& sinks) override;
    size_t estimateSize() const override { return data.size() + topic.size() + type.size() + 16; }
};
struct SinkConfig {
    std::string module_name;
    std::string pattern;
    size_t max_bytes;
    std::chrono::minutes max_age;
    size_t reserve_n;
    bool compress_old;
};
// Sink 工厂（依赖注入，降低耦合）
class SinkFactory {
public:
    virtual ~SinkFactory() = default;
    virtual std::shared_ptr<ILogSink> createSink(
        const std::filesystem::path& base_dir, 
        const ModuleConfig& config,
        const std::string& sink_type) = 0;
};


class LoggerCore {
public:
    // 获取单例实例

    static LoggerCore& instance();
    
    // 从配置文件初始化
    void initFromConfig(const std::string& config_path, 
                       std::unique_ptr<SinkFactory> factory = nullptr);
    
    // 从配置对象初始化
    void initFromConfig(const LoggerConfig& config,
                       std::unique_ptr<SinkFactory> factory = nullptr);
    
    // 传统初始化（向后兼容）
    void initSinks(const std::filesystem::path& base_dir,
                  std::unique_ptr<SinkFactory> factory = nullptr);
    
    // 运行时调整
    void setLogLevel(LogLevel level);
    void setAsyncMode(bool enable);
    void reloadConfig(const std::string& config_path);
    
    
    // 写文本日志
    void log(LogLevel level, const std::string& message,
            const std::string& file, const std::string& function, int line);
    
    // 写二进制日志
    void logBinary(const void* data, size_t size, const std::string& tag = "binary");
    
    // 写消息记录
    void recordMessage(const std::string& topic, const std::string& type,
                      const std::vector<uint8_t>& data);
    
    ~LoggerCore();
    //查询当前配置
    LoggerConfig getCurrentConfig() const;
    
private:
    LoggerCore();
    LoggerCore(const LoggerCore&) = delete;
    LoggerCore& operator=(const LoggerCore&) = delete;
    

    // 核心写入逻辑（同步）
    void processEntry(std::unique_ptr<ILogEntry> entry);
    
    // 异步队列管理
    void enqueueAsync(std::unique_ptr<ILogEntry> entry);
    void processAsyncQueue();
    
    // 辅助函数
    std::string getCurrentTime();
    std::string logLevelToString(LogLevel level);
    // 成员变量
    std::map<std::string, std::shared_ptr<ILogSink>> sinks_;
    LoggerConfig current_config_;
    std::atomic<LogLevel> current_level_{LogLevel::INFO};
    
    // 异步模式
    std::atomic<bool> async_mode_{false};
    std::atomic<bool> stop_{false};
    std::thread worker_;
    
    
    


    // 双缓冲（正确实现）
    std::queue<std::unique_ptr<ILogEntry>> queue_;
    size_t max_queue_size_ = 10000;
    std::mutex queue_mtx_;
    std::condition_variable cv_;
    
    // 同步写入锁（与异步分离）
    std::mutex sync_write_mtx_;
    mutable std::mutex config_mtx_;

};

// 便捷宏
#define LOG_DEBUG(msg) LoggerCore::instance().log(LogLevel::DEBUG, msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_INFO(msg) LoggerCore::instance().log(LogLevel::INFO, msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_WARNING(msg) LoggerCore::instance().log(LogLevel::WARNING, msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_ERROR(msg) LoggerCore::instance().log(LogLevel::ERROR, msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_CRITICAL(msg) LoggerCore::instance().log(LogLevel::CRITICAL, msg, __FILE__, __FUNCTION__, __LINE__)