#include "LoggerCore.h"
#include "TextRollingFileSink.h"
#include "BinaryRollingFileSink.h"
#include "BagSink.h"
#include <iostream>
#include <sstream>
#include <iomanip>


void LogEntry::process(LoggerCore* core) {
    core->logSync(*this);
}

void BinaryEntry::process(LoggerCore* core) {
    core->logBinarySync(*this);
}

void MessageEntry::process(LoggerCore* core) {
    core->recordMessageSync(*this);
}




LoggerCore::LoggerCore() {}

LoggerCore::~LoggerCore() {
    stop_ = true;
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

LoggerCore& LoggerCore::instance() {
    static LoggerCore inst;
    return inst;
}

void LoggerCore::initSinks(const std::filesystem::path& base_dir) {
    // 文本日志模块
    sinks_["text"] = std::make_shared<TextRollingFileSink>(
        base_dir, "text",
        "log_%Y%m%d_%H%M%S_%03d.txt",
        1 * 1024 * 1024,  // 1MB
        std::chrono::minutes(60),
        8, true
    );
    
    // 二进制日志模块
    sinks_["binary"] = std::make_shared<BinaryRollingFileSink>(
        base_dir, "binary",
        "binary_%Y%m%d_%H%M%S_%03d.bin",
        5 * 1024 * 1024,  // 5MB
        std::chrono::minutes(120),
        5, true
    );
    
    // 消息包模块
    sinks_["bag"] = std::make_shared<BagSink>(
        base_dir, "bag",
        "messages_%Y%m%d_%H%M%S_%03d.bag",
        10 * 1024 * 1024,  // 10MB
        std::chrono::minutes(180),
        3, true
    );
}

void LoggerCore::setLogLevel(LogLevel level) {
    current_level_ = level;
}

void LoggerCore::setAsyncMode(bool enable) {
    async_mode_ = enable;
    if (async_mode_ && !worker_started_) {
        startAsyncWorker();
    }
}

void LoggerCore::log(LogLevel level, const std::string& message,
                    const std::string& file, const std::string& function, int line) {
    if (static_cast<int>(level) < static_cast<int>(current_level_)) {
        return;
    }
    
    LogEntry entry{level, message, file, function, getCurrentTime(), line};
    
    if (async_mode_) {
        logAsync(entry);
    } else {
        logSync(entry);
    }
}

void LoggerCore::logBinary(const void* data, size_t size, const std::string& tag) {
    BinaryEntry entry;
    entry.data.assign((uint8_t*)data, (uint8_t*)data + size);
    entry.tag = tag;
    entry.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    if (async_mode_) {
        logBinaryAsync(entry);
    } else {
        logBinarySync(entry);
    }
}

void LoggerCore::recordMessage(const std::string& topic, const std::string& type,
                              const std::vector<uint8_t>& data) {
    MessageEntry entry{topic, type, data, 
        static_cast<uint64_t>(std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now()))};
    
    if (async_mode_) {
        recordMessageAsync(entry);
    } else {
        recordMessageSync(entry);
    }
}

void LoggerCore::logSync(const LogEntry& entry) {
    std::string formatted = formatLogEntry(entry);
    
    std::lock_guard<std::mutex> lock(sync_mtx_);
    std::cout << formatted << std::endl;
    
    if (sinks_.count("text")) {
        sinks_["text"]->writeText(formatted);
    }
}

void LoggerCore::logBinarySync(const BinaryEntry& entry) {
    std::lock_guard<std::mutex> lock(sync_mtx_);
    
    if (sinks_.count("binary")) {
        sinks_["binary"]->writeBinary(entry.data, entry.tag, entry.timestamp);
    }
}

void LoggerCore::recordMessageSync(const MessageEntry& entry) {
    std::lock_guard<std::mutex> lock(sync_mtx_);
    
    if (sinks_.count("bag")) {
        sinks_["bag"]->writeMessage(entry.topic, entry.type, entry.data, entry.timestamp);
    }
}

void LoggerCore::logAsync(const LogEntry& entry) {
    {
        std::lock_guard<std::mutex> lock(buffer_mtx_);
        text_front_.push_back(entry);
    }
    cv_.notify_one();
}

void LoggerCore::logBinaryAsync(const BinaryEntry& entry) {
    {
        std::lock_guard<std::mutex> lock(buffer_mtx_);
        binary_front_.push_back(entry);
    }
    cv_.notify_one();
}

void LoggerCore::recordMessageAsync(const MessageEntry& entry) {
    {
        std::lock_guard<std::mutex> lock(buffer_mtx_);
        message_front_.push_back(entry);
    }
    cv_.notify_one();
}

void LoggerCore::startAsyncWorker() {
    worker_started_ = true;
    worker_ = std::thread(&LoggerCore::processLogs, this);
}

void LoggerCore::processLogs() {
    while (!stop_) {
        {
            std::unique_lock<std::mutex> lock(buffer_mtx_);
            cv_.wait_for(lock, std::chrono::milliseconds(100), [this] {
                return !text_front_.empty() || !binary_front_.empty() || 
                       !message_front_.empty() || stop_;
            });
            
            entry_front_.swap(entry_back_);
        }
        
    
        if (!entry_back_.empty()) {
            for (auto& entry : entry_back_) {
                entry->process(this);  // 多态
            }
            entry_back_.clear();
        }
    }
    
    // 刷新残留数据
    for (auto& entry : entry_front_) {
        entry->process(this);
    }
}

std::string LoggerCore::formatLogEntry(const LogEntry& entry) {
    std::ostringstream oss;
    oss << entry.timestamp << " " << logLevelToString(entry.level)
        << " " << entry.file << ":" << entry.line << " "
        << entry.function << " - " << entry.message;
    return oss.str();
}

std::string LoggerCore::getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string LoggerCore::logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}