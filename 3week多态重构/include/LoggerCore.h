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
class LoggerCore;

enum class LogLevel { DEBUG, INFO, WARNING, ERROR, CRITICAL };

class ILogEntry {
public:
    virtual ~ILogEntry() = default;
    virtual void process(LoggerCore* core) = 0;  
};
class LogEntry : public ILogEntry {
public:
    LogLevel level;
    std::string message;
    std::string file;
    std::string function;
    std::string timestamp;
    int line;


    LogEntry(LogLevel level, const std::string& message, const std::string& file, 
         const std::string& function, const std::string& timestamp, int line)
    : level(level), message(message), file(file), function(function), timestamp(timestamp), line(line) {}

    
    void process(LoggerCore* core) override;
};
class BinaryEntry : public ILogEntry {
public:
    std::vector<uint8_t> data;
    std::string tag;
    uint64_t timestamp;
    // 默认构造函数
    
    BinaryEntry(const std::vector<uint8_t>& data,const std::string& tag, uint64_t timestamp):
    data(data), tag(tag), timestamp(timestamp) {}
    void process(LoggerCore* core) override;
};

class MessageEntry : public ILogEntry {
public:
    std::string topic;
    std::string type;
    std::vector<uint8_t> data;
    uint64_t timestamp;
    MessageEntry(const std::string& topic, const std::string& type,
             const std::vector<uint8_t>& data, uint64_t timestamp)
    : topic(topic), type(type), data(data), timestamp(timestamp) {}

    void process(LoggerCore* core) override;
};




class LoggerCore {
public:
    // 异步写入

    void recordMessageSync(const MessageEntry& entry);
    void logSync(const LogEntry& entry);    
    void logBinarySync(const BinaryEntry& entry);




    static LoggerCore& instance();
    
    // 初始化各模块 Sink
    void initSinks(const std::filesystem::path& base_dir);
    
    // 设置日志级别
    void setLogLevel(LogLevel level);
    
    // 切换同步/异步模式
    void setAsyncMode(bool enable);
    
    // 写文本日志
    void log(LogLevel level, const std::string& message,
            const std::string& file, const std::string& function, int line);
    
    // 写二进制日志
    void logBinary(const void* data, size_t size, const std::string& tag = "binary");
    
    // 写消息记录
    void recordMessage(const std::string& topic, const std::string& type,
                      const std::vector<uint8_t>& data);
    
    ~LoggerCore();
    
private:
    LoggerCore();
    LoggerCore(const LoggerCore&) = delete;
    LoggerCore& operator=(const LoggerCore&) = delete;
    

    
    // 异步写入
    void logAsync(const LogEntry& entry);
    void logBinaryAsync(const BinaryEntry& entry);
    void recordMessageAsync(const MessageEntry& entry);
    
    // 异步工作线程
    void startAsyncWorker();
    void processLogs();

    // 格式化文本日志
    std::string formatLogEntry(const LogEntry& entry);
    std::string getCurrentTime();
    std::string logLevelToString(LogLevel level);
    
    // Sink 管理
    std::map<std::string, std::shared_ptr<ILogSink>> sinks_;
    
    LogLevel current_level_ = LogLevel::INFO;
    bool async_mode_ = false;
    std::atomic<bool> stop_{false};
    bool worker_started_ = false;
    
    // 使用统一的队列存储所有类型的条目
    std::vector<std::unique_ptr<ILogEntry>> entry_front_;
    std::vector<std::unique_ptr<ILogEntry>> entry_back_;
    std::vector<std::unique_ptr<LogEntry>> text_front_;
    std::vector<std::unique_ptr<BinaryEntry>> binary_front_;
    std::vector<std::unique_ptr<MessageEntry>> message_front_;


    std::mutex buffer_mtx_;
    std::mutex sync_mtx_;
    std::thread worker_;
    std::condition_variable cv_;



};

// 便捷宏
#define LOG_DEBUG(msg) LoggerCore::instance().log(LogLevel::DEBUG, msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_INFO(msg) LoggerCore::instance().log(LogLevel::INFO, msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_WARNING(msg) LoggerCore::instance().log(LogLevel::WARNING, msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_ERROR(msg) LoggerCore::instance().log(LogLevel::ERROR, msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_CRITICAL(msg) LoggerCore::instance().log(LogLevel::CRITICAL, msg, __FILE__, __FUNCTION__, __LINE__)