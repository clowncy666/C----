#include "../../logger/include/logger/LoggerConfig.h"
#include "../../logger/src/core/LoggerCore.h"
#include <filesystem>
#include <iostream>
#include "../../logger/include/logger/Logger.h" 

namespace logger {

// PIMPL实现类
class Logger::Impl {
public:
    LoggerCore& core;
    std::string config_path_;
    bool initialized_ = false;
    
    Impl() : core(LoggerCore::instance()) {}
    
    void findAndLoadConfig() {
        const char* search_paths[] = {
            "./logger_config.json",
            "../logger_config.json",
            "/etc/logger_config.json",
            nullptr
        };
        
        for (int i = 0; search_paths[i]; ++i) {
            if (std::filesystem::exists(search_paths[i])) {
                config_path_ = search_paths[i];
                core.initFromConfig(config_path_);
                initialized_ = true;
                std::cout << "[Logger] Loaded config from: " << config_path_ << std::endl;
                return;
            }
        }
        
        // 使用默认配置
        core.initSinks("./logs");
        initialized_ = true;
        std::cout << "[Logger] Using default configuration" << std::endl;
    }
};

Logger::Logger() : pimpl_(std::make_unique<Impl>()) {}
Logger::~Logger() = default;

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::init() {
    pimpl_->findAndLoadConfig();
}

void Logger::init(const std::string& config_path) {
    pimpl_->config_path_ = config_path;
    pimpl_->core.initFromConfig(config_path);
    pimpl_->initialized_ = true;
}

void Logger::init(const LoggerConfig& config) {
    pimpl_->core.initFromConfig(config);
    pimpl_->initialized_ = true;
}

void Logger::debug(const std::string& msg, const char* file, const char* func, int line) {
    if (!pimpl_->initialized_) init();
    pimpl_->core.log(LogLevel::DEBUG, msg, file, func, line);
}

void Logger::info(const std::string& msg, const char* file, const char* func, int line) {
    if (!pimpl_->initialized_) init();
    pimpl_->core.log(LogLevel::INFO, msg, file, func, line);
}

void Logger::warning(const std::string& msg, const char* file, const char* func, int line) {
    if (!pimpl_->initialized_) init();
    pimpl_->core.log(LogLevel::WARNING, msg, file, func, line);
}

void Logger::error(const std::string& msg, const char* file, const char* func, int line) {
    if (!pimpl_->initialized_) init();
    pimpl_->core.log(LogLevel::ERROR, msg, file, func, line);
}

void Logger::critical(const std::string& msg, const char* file, const char* func, int line) {
    if (!pimpl_->initialized_) init();
    pimpl_->core.log(LogLevel::CRITICAL, msg, file, func, line);
}

void Logger::binary(const void* data, size_t size, const std::string& tag) {
    if (!pimpl_->initialized_) init();
    pimpl_->core.logBinary(data, size, tag);
}

void Logger::message(const std::string& topic, const std::string& type, 
                    const std::vector<uint8_t>& data) {
    if (!pimpl_->initialized_) init();
    pimpl_->core.recordMessage(topic, type, data);
}

void Logger::setLevel(LogLevel level) {
    pimpl_->core.setLogLevel(level);
}

void Logger::setAsync(bool enable) {
    pimpl_->core.setAsyncMode(enable);
}

void Logger::reload() {
    if (!pimpl_->config_path_.empty()) {
        pimpl_->core.reloadConfig(pimpl_->config_path_);
    } else {
        std::cerr << "[Logger] No config path to reload" << std::endl;
    }
}

void Logger::flush() {
    // 可以添加刷新所有Sink的逻辑
}

LoggerConfig Logger::getConfig() const {
    return pimpl_->core.getCurrentConfig();
}

bool Logger::isInitialized() const {
    return pimpl_->initialized_;
}

}