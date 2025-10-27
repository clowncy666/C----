#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>
#include <vector>
#include <atomic>

enum class LogLevel {
    DEBUG, INFO, WARNING, ERROR, CRITICAL
};

struct LogEntry {
    LogLevel level;
    std::string message;
    std::string file;
    std::string function;
    int line;
    std::string timestamp;
};

class AsyncLogger {
public:
    static AsyncLogger& instance() {
        static AsyncLogger instance;
        return instance;
    }

    void setLogLevel(LogLevel level) {
        current_level = level;
    }

    void log(LogLevel level, const std::string& message,
             const std::string& file, const std::string& function, int line) {

        if (static_cast<int>(level) < static_cast<int>(current_level)) {
            return; // 不记录低于当前等级的日志
        }

        LogEntry entry;
        entry.level = level;
        entry.message = message;
        entry.file = file;
        entry.function = function;
        entry.line = line;
        entry.timestamp = getCurrentTime();

        {
            std::lock_guard<std::mutex> lock(buffer_mutex);
            front_buffer.push_back(std::move(entry));
        }

        cv.notify_one();
    }

    ~AsyncLogger() {
        stop = true;
        cv.notify_all();
        if (worker.joinable()) {
            worker.join();
        }
        if (log_file.is_open()) {
            log_file.close();
        }
    }

private:
    LogLevel current_level = LogLevel::INFO;
    std::ofstream log_file;

    std::mutex buffer_mutex;
    std::condition_variable cv;
    std::vector<LogEntry> front_buffer;
    std::vector<LogEntry> back_buffer;
    std::thread worker;
    std::atomic<bool> stop{false};

    AsyncLogger() {
        openLogFile();
        worker = std::thread(&AsyncLogger::processLogs, this);
    }

    AsyncLogger(const AsyncLogger&) = delete;
    AsyncLogger& operator=(const AsyncLogger&) = delete;

    // 打开日志文件
    void openLogFile() {
        log_file.open("log.txt", std::ios_base::app);
        if (!log_file.is_open()) {
            std::cerr << "Failed to open log file!" << std::endl;
        }
    }

    // 获取当前时间
    std::string getCurrentTime() {
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&t);
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

    // 主循环线程
    void processLogs() {
        while (!stop) {
            {
                std::unique_lock<std::mutex> lock(buffer_mutex);
                cv.wait_for(lock, std::chrono::milliseconds(500), [this] {
                    return !front_buffer.empty() || stop;
                });
                front_buffer.swap(back_buffer);
            }

            if (!back_buffer.empty()) {
                for (const auto& entry : back_buffer) {
                    writeLog(entry);
                }
                back_buffer.clear();
            }
        }

        // 程序退出前，处理剩余日志
        for (const auto& entry : front_buffer) {
            writeLog(entry);
        }
    }

    // 实际写入
    void writeLog(const LogEntry& entry) {
        std::ostringstream oss;
        oss << entry.timestamp << " " << logLevelToString(entry.level)
            << " " << entry.file << ":" << entry.line << " "
            << entry.function << " - " << entry.message;

        std::string log_msg = oss.str();

        std::cout << log_msg << std::endl;
        if (log_file.is_open()) {
            log_file << log_msg << std::endl;
        }
    }
};

// 简化宏定义
#define LOG_DEBUG(msg) AsyncLogger::instance().log(LogLevel::DEBUG, msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_INFO(msg) AsyncLogger::instance().log(LogLevel::INFO, msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_WARNING(msg) AsyncLogger::instance().log(LogLevel::WARNING, msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_ERROR(msg) AsyncLogger::instance().log(LogLevel::ERROR, msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_CRITICAL(msg) AsyncLogger::instance().log(LogLevel::CRITICAL, msg, __FILE__, __FUNCTION__, __LINE__)

// 测试
int main() {
    auto& logger = AsyncLogger::instance();
    logger.setLogLevel(LogLevel::DEBUG);

    for (int i = 0; i < 10; ++i) {
        LOG_INFO("Log message " + std::to_string(i));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    LOG_ERROR("An error occurred!");
    LOG_DEBUG("Debug message!");
}
