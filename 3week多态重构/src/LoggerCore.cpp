#include "LoggerCore.h"
#include "TextRollingFileSink.h"
#include "BinaryRollingFileSink.h"
#include "BagSink.h"
#include <iostream>
#include <sstream>
#include <iomanip>

// ILogEntry 实现（多态的核心）
void TextLogEntry::writeTo(const std::map<std::string, std::shared_ptr<ILogSink>>& sinks) {
    auto it = sinks.find("text");
    if (it != sinks.end()) {
        // 格式化在这里完成，减少耦合
        std::ostringstream oss;
        oss << timestamp << " ";
        
        switch (level) {
            case LogLevel::DEBUG: oss << "DEBUG"; break;
            case LogLevel::INFO: oss << "INFO"; break;
            case LogLevel::WARNING: oss << "WARNING"; break;
            case LogLevel::ERROR: oss << "ERROR"; break;
            case LogLevel::CRITICAL: oss << "CRITICAL"; break;
        }
        
        oss << " " << file << ":" << line << " "
            << function << " - " << message;
        
        std::string formatted = oss.str();
        
        // 控制台输出
        std::cout << formatted << std::endl;
        
        // 写入 Sink
        it->second->writeText(formatted);
    }
}

void BinaryLogEntry::writeTo(const std::map<std::string, std::shared_ptr<ILogSink>>& sinks) {
    auto it = sinks.find("binary");
    if (it != sinks.end()) {
        it->second->writeBinary(data, tag, timestamp);
    }
}

void MessageLogEntry::writeTo(const std::map<std::string, std::shared_ptr<ILogSink>>& sinks) {
    auto it = sinks.find("bag");
    if (it != sinks.end()) {
        it->second->writeMessage(topic, type, data, timestamp);
    }
}

class DefaultSinkFactory : public SinkFactory {
public:
    std::shared_ptr<ILogSink> createTextSink(
        const std::filesystem::path& base_dir, const SinkConfig& config) override {
        return std::make_shared<TextRollingFileSink>(
            base_dir, config.module_name, config.pattern,
            config.max_bytes, config.max_age, config.reserve_n, config.compress_old
        );
    }
    
    std::shared_ptr<ILogSink> createBinarySink(
        const std::filesystem::path& base_dir, const SinkConfig& config) override {
        return std::make_shared<BinaryRollingFileSink>(
            base_dir, config.module_name, config.pattern,
            config.max_bytes, config.max_age, config.reserve_n, config.compress_old
        );
    }
    
    std::shared_ptr<ILogSink> createBagSink(
        const std::filesystem::path& base_dir, const SinkConfig& config) override {
        return std::make_shared<BagSink>(
            base_dir, config.module_name, config.pattern,
            config.max_bytes, config.max_age, config.reserve_n, config.compress_old
        );
    }
};

LoggerCore::LoggerCore() {front_buffer_.reserve(1024);
    back_buffer_.reserve(1024);}

LoggerCore::~LoggerCore() {
    stop_ = true;
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
    // 处理残留数据
    std::lock_guard<std::mutex> lock(buffer_mtx_);
    for (auto& entry : front_buffer_) {
        if (entry) {
            entry->writeTo(sinks_);
        }
    }
}

LoggerCore& LoggerCore::instance() {
    static LoggerCore inst;
    return inst;
}

void LoggerCore::initSinks(const std::filesystem::path& base_dir,std::unique_ptr<SinkFactory> factory) {
    if (!factory) {
        factory = std::make_unique<DefaultSinkFactory>();
    }
    
    // 配置可以从外部传入（进一步降低耦合）
    SinkConfig text_config{
        "text", "log_%Y%m%d_%H%M%S_%03d.txt",
        1 * 1024 * 1024, std::chrono::minutes(60), 8, true
    };
    
    SinkConfig binary_config{
        "binary", "binary_%Y%m%d_%H%M%S_%03d.bin",
        5 * 1024 * 1024, std::chrono::minutes(120), 5, true
    };
    
    SinkConfig bag_config{
        "bag", "messages_%Y%m%d_%H%M%S_%03d.bag",
        10 * 1024 * 1024, std::chrono::minutes(180), 3, true
    };
    
    sinks_["text"] = factory->createTextSink(base_dir, text_config);
    sinks_["binary"] = factory->createBinarySink(base_dir, binary_config);
    sinks_["bag"] = factory->createBagSink(base_dir, bag_config);
}


void LoggerCore::setLogLevel(LogLevel level) {
    current_level_ = level;
}

void LoggerCore::setAsyncMode(bool enable) {
    if (enable && !async_mode_) {
        async_mode_ = true;
        worker_ = std::thread(&LoggerCore::processAsyncQueue, this);
    } else if (!enable && async_mode_) {
        stop_ = true;
        cv_.notify_all();
        if (worker_.joinable()) {
            worker_.join();
        }
        stop_ = false;
        async_mode_ = false;
    }
}

void LoggerCore::log(LogLevel level, const std::string& message,
                    const std::string& file, const std::string& function, int line) {
    if (static_cast<int>(level) < static_cast<int>(current_level_)) {
        return;
    }
    
    std::make_unique<TextLogEntry> entry{level, message, file, function, getCurrentTime(), line};
    
    if (async_mode_) {
        enqueueAsync(std::move(entry));
    } else {
        processEntry(std::move(entry));
    }
}

void LoggerCore::logBinary(const void* data, size_t size, const std::string& tag) {
    std::vector<uint8_t> data_vec((uint8_t*)data, (uint8_t*)data + size);
    uint64_t timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    auto entry = std::make_unique<BinaryLogEntry>(
        std::move(data_vec), tag, timestamp
    );
    
    if (async_mode_) {
        enqueueAsync(std::move(entry));
    } else {
        processEntry(std::move(entry));
    }
}

void LoggerCore::recordMessage(const std::string& topic, const std::string& type,
                              const std::vector<uint8_t>& data){
    // ✅ 修复：先定义 timestamp，然后创建 entry
    uint64_t timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    auto entry = std::make_unique<MessageLogEntry>(
        topic, type, data, timestamp
    );
    if (async_mode_) {
        enqueueAsync(std::move(entry));
    } else {
        processEntry(std::move(entry));
    }
}

void LoggerCore::processEntry(std::unique_ptr<ILogEntry> entry) {
    if (!entry) return;
    
    // 同步模式直接写入，使用独立的锁
    std::lock_guard<std::mutex> lock(sync_write_mtx_);
    entry->writeTo(sinks_);
}

void LoggerCore::enqueueAsync(std::unique_ptr<ILogEntry> entry) {
    if (!entry) return;
    
    {
        std::lock_guard<std::mutex> lock(buffer_mtx_);
        front_buffer_.push_back(std::move(entry));
    }
    cv_.notify_one();
}

void LoggerCore::processAsyncQueue() {
    while (!stop_) {
        // 等待数据或停止信号
        {
            std::unique_lock<std::mutex> lock(buffer_mtx_);
            cv_.wait_for(lock, std::chrono::milliseconds(100), [this] {
                return !front_buffer_.empty() || stop_;
            });
            
            // 交换缓冲区（关键：正确的双缓冲实现）
            front_buffer_.swap(back_buffer_);
        }
        
        // 在锁外处理数据（提高并发性能）
        for (auto& entry : back_buffer_) {
            if (entry) {
                entry->writeTo(sinks_);
            }
        }
        back_buffer_.clear();
    }
    
    // 处理残留数据
    std::lock_guard<std::mutex> lock(buffer_mtx_);
    for (auto& entry : front_buffer_) {
        if (entry) {
            entry->writeTo(sinks_);
        }
    }
    front_buffer_.clear();
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