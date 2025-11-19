#include "LoggerCore.h"

#include "../manager/RollingFileManager.h"
#include "../sinks/TextRollingFileSink.h"    
#include "../sinks/BinaryRollingFileSink.h"   
#include "../sinks/BagSink.h" 
#include <iostream>
#include <sstream>
#include <iomanip>

// ============================================
// ILogEntry 实现（多态的核心）
// ============================================
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

// ============================================
// 默认 Sink 工厂实现
// ============================================
class DefaultSinkFactory : public SinkFactory {
public:
    std::shared_ptr<ILogSink> createSink(
        const std::filesystem::path& base_dir, 
        const ModuleConfig& config,
        const std::string& sink_type) override
    {
        if (sink_type == "text") {
            return std::make_shared<TextRollingFileSink>(
                base_dir, config.name, config.pattern,
                config.max_bytes, config.max_age, config.reserve_n, config.compress_old
            );
        } else if (sink_type == "binary") {
            return std::make_shared<BinaryRollingFileSink>(
                base_dir, config.name, config.pattern,
                config.max_bytes, config.max_age, config.reserve_n, config.compress_old
            );
        } else if (sink_type == "bag") {
            return std::make_shared<BagSink>(
                base_dir, config.name, config.pattern,
                config.max_bytes, config.max_age, config.reserve_n, config.compress_old
            );
        }
        
        throw std::runtime_error("Unknown sink type: " + sink_type);
    }
};

// ============================================
// LoggerCore 实现
// ============================================
LoggerCore::LoggerCore() {
    queue_ = std::queue<std::unique_ptr<ILogEntry>>();
}

LoggerCore::~LoggerCore() {
    stop_ = true;
    cv_.notify_all();
    
    if (worker_.joinable()) {
        worker_.join();
    }
    
    // 处理残留数据
    std::lock_guard<std::mutex> lock(queue_mtx_);
    while (!queue_.empty()) {
        auto& entry = queue_.front();
        if (entry) {
            entry->writeTo(sinks_);
        }
        queue_.pop();
    }
}

LoggerCore& LoggerCore::instance() {
    static LoggerCore inst;
    return inst;
}

void LoggerCore::initFromConfig(const std::string& config_path, 
                                std::unique_ptr<SinkFactory> factory) 
{
    try {
        LoggerConfig config = LoggerConfig::fromFile(config_path);
        initFromConfig(config, std::move(factory));
        std::cout << "[Logger] Loaded config from: " << config_path << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[Logger] Failed to load config: " << e.what() 
                  << "\nUsing default configuration." << std::endl;
        initSinks("./logs");
    }
}

void LoggerCore::initFromConfig(const LoggerConfig& config,
                                std::unique_ptr<SinkFactory> factory) 
{
    std::lock_guard<std::mutex> lock(config_mtx_);
    
    if (!factory) {
        factory = std::make_unique<DefaultSinkFactory>();
    }
    
    // 更新配置
    current_config_ = config;
    current_level_.store(config.log_level);
    max_queue_size_ = config.async_queue_size;
    
    // 清空旧 Sink
    sinks_.clear();
    
    // 创建新 Sink
    for (const auto& mod_config : config.modules) {
        std::string sink_type = "text"; // 默认类型
        
        // 根据模块名推断类型（可改进为配置项）
        if (mod_config.name == "text") {
            sink_type = "text";
        } else if (mod_config.name == "binary") {
            sink_type = "binary";
        } else if (mod_config.name == "bag") {
            sink_type = "bag";
        }
        
        try {
            auto sink = factory->createSink(config.base_dir, mod_config, sink_type);
            sinks_[mod_config.name] = sink;
            std::cout << "[Logger] Created sink: " << mod_config.name << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[Logger] Failed to create sink " << mod_config.name 
                      << ": " << e.what() << std::endl;
        }
    }
    
    // 设置异步模式
    setAsyncMode(config.async_mode);
}






void LoggerCore::initSinks(const std::filesystem::path& base_dir, 
                           std::unique_ptr<SinkFactory> factory) {
    
    
    // 使用默认配置
    LoggerConfig default_config;
    default_config.base_dir = base_dir;
    default_config.modules = {
        ModuleConfig{
            "text", "log_%Y%m%d_%H%M%S_%03d.txt",
            1 * 1024 * 1024, std::chrono::minutes(60), 8, true
        },
        ModuleConfig{
            "binary", "binary_%Y%m%d_%H%M%S_%03d.bin",
            5 * 1024 * 1024, std::chrono::minutes(120), 5, true
        },
        ModuleConfig{
            "bag", "messages_%Y%m%d_%H%M%S_%03d.bag",
            10 * 1024 * 1024, std::chrono::minutes(180), 3, true
        }
    };
    
    initFromConfig(default_config, std::move(factory));
}


void LoggerCore::setLogLevel(LogLevel level) {
    current_level_ = level;
}

void LoggerCore::setAsyncMode(bool enable) {
    if (enable && !async_mode_) {
        async_mode_ = true;
        worker_ = std::thread(&LoggerCore::processAsyncQueue, this);
        std::cout << "[Logger] Async mode enabled (queue size: " 
                  << max_queue_size_ << ")" << std::endl;
    } else if (!enable && async_mode_) {
        stop_ = true;
        cv_.notify_all();
        if (worker_.joinable()) {
            worker_.join();
        }
        stop_ = false;
        async_mode_ = false;
        std::cout << "[Logger] Async mode disabled" << std::endl;
    }
}


void LoggerCore::reloadConfig(const std::string& config_path) {
    try {
        LoggerConfig new_config = LoggerConfig::fromFile(config_path);
        
        // 先停止异步模式
        bool was_async = async_mode_;
        if (was_async) {
            setAsyncMode(false);
        }
        
        // 重新初始化
        initFromConfig(new_config);
        
        // 恢复异步模式
        if (was_async) {
            setAsyncMode(true);
        }
        
        std::cout << "[Logger] Configuration reloaded from: " << config_path << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[Logger] Failed to reload config: " << e.what() << std::endl;
    }
}

LoggerConfig LoggerCore::getCurrentConfig() const {
    std::lock_guard<std::mutex> lock(config_mtx_);
    return current_config_;
}



void LoggerCore::log(LogLevel level, const std::string& message,
                     const std::string& file, const std::string& function, int line) {
    if (static_cast<int>(level) < static_cast<int>(current_level_.load())) {
        return;
    }
    

    auto entry = std::make_unique<TextLogEntry>(
        level, message, file, function, getCurrentTime(), line
    );
    
    if (async_mode_) {
        enqueueAsync(std::move(entry));
    } else {
        processEntry(std::move(entry));
    }
}

void LoggerCore::logBinary(const void* data, size_t size, const std::string& tag) {
    std::vector<uint8_t> data_vec(static_cast<const uint8_t*>(data), 
                                  static_cast<const uint8_t*>(data) + size);
    
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
                               const std::vector<uint8_t>& data) {
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
        std::lock_guard<std::mutex> lock(queue_mtx_);
        if (queue_.size() >= max_queue_size_) {
            queue_.pop();
            static size_t drop_count = 0;
            if (++drop_count % 1000 == 0) {
                std::cerr << "[Logger] Queue overflow, dropped " 
                          << drop_count << " entries" << std::endl;
            }
        }
        
        queue_.push(std::move(entry));
    }
    cv_.notify_one();
}

void LoggerCore::processAsyncQueue() {
    std::vector<std::unique_ptr<ILogEntry>> batch;
    batch.reserve(100); 
    while (!stop_) {
        // 等待数据或停止信号
        {
            std::unique_lock<std::mutex> lock(queue_mtx_);
            cv_.wait_for(lock, std::chrono::milliseconds(100), [this] {
                return !queue_.empty() || stop_;
            });
            
            while (!queue_.empty() && batch.size() < 100) {
                batch.push_back(std::move(queue_.front()));
                queue_.pop();
            }
        }
        
        // 在锁外处理数据（提高并发性能）
        for (auto& entry : batch) {
            if (entry) {
                entry->writeTo(sinks_);
            }
        }
        batch.clear();
    }
    
    // 处理残留数据
    std::lock_guard<std::mutex> lock(queue_mtx_);
    while (!queue_.empty()) {
        auto& entry = queue_.front();
        if (entry) {
            entry->writeTo(sinks_);
        }
        queue_.pop();
    }
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