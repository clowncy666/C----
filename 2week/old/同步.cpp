#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <mutex>
#include <thread>

class Logger {
public:
    // 枚举定义
    enum class LogLevel {
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        CRITICAL
    };

    Logger() : current_level(LogLevel::INFO) {}

    // 设置当前的日志级别
    void setLogLevel(LogLevel level) {
        current_level = level;
    }

    // 日志记录函数
    void log(LogLevel level, const std::string& message,
             const std::string& file, const std::string& function, int line) {

        if (static_cast<int>(level) < static_cast<int>(current_level)) {
            return; // 如果日志级别低于当前级别，则不记录
        }

        std::ostringstream log_stream;
        log_stream << getCurrentTime() << " " << logLevelToString(level)
                   << " " << file << ":" << line << " " << function
                   << " - " << message;
        
        // 同步日志写入（后面会改为异步）
        std::lock_guard<std::mutex> lock(mtx); // 线程安全
        std::cout << log_stream.str() << std::endl;

        // 写入到文件
        std::ofstream log_file("log.txt", std::ios_base::app);
        if (log_file.is_open()) {
            log_file << log_stream.str() << std::endl;
        }
    }

private:
    LogLevel current_level;
    std::mutex mtx;

    // 获取当前时间戳
    std::string getCurrentTime() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&time);
        
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    // 日志级别转字符串
    std::string logLevelToString(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARNING: return "WARNING";
            case LogLevel::ERROR: return "ERROR";
            case LogLevel::CRITICAL: return "CRITICAL";
            default: return "UNKNOWN";
        }
    }
};
#define LOG(level, message) \
    logger.log(level, message, __FILE__, __FUNCTION__, __LINE__)
