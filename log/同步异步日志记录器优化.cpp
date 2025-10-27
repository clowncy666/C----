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

class Logger {
public:
    enum class LogLevel { DEBUG, INFO, WARNING, ERROR, CRITICAL };

    // 单例获取实例
    static Logger& instance() {
        static Logger instance;
        return instance;
    }

    // 设置日志级别
    void setLogLevel(LogLevel level) {
        current_level = level;
    }

    // 切换模式
    void setAsyncMode(bool enable) {
        async_mode = enable;
        if (async_mode && !worker_started) {
            startAsyncWorker();
        }
    }

    // 主接口：写日志
    void log(LogLevel level, const std::string& message,
             const std::string& file, const std::string& function, int line) {
        if (static_cast<int>(level) < static_cast<int>(current_level)) {
            return;
        }

        if (async_mode) {
            logAsync(level, message, file, function, line);
        } else {
            logSync(level, message, file, function, line);
        }
    }

    ~Logger() {
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
    struct LogEntry {
        LogLevel level;
        std::string message, file, function, timestamp;
        int line;
    };

    LogLevel current_level = LogLevel::INFO;
    bool async_mode = false;
    std::atomic<bool> stop{false};
    bool worker_started = false;

    // 同步部分
    std::mutex sync_mtx;

    // 异步部分
    std::mutex buffer_mutex;
    std::condition_variable cv;
    std::vector<LogEntry> front_buffer, back_buffer;
    std::thread worker;
    std::ofstream log_file;

    Logger() { openLogFile(); }
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // 获取当前时间
    std::string getCurrentTime() {
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&t);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    // 日志级别字符串
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

    // 打开文件
    void openLogFile() {
        log_file.open("log.txt", std::ios_base::app);
        if (!log_file.is_open()) {
            std::cerr << "Failed to open log file!" << std::endl;
        }
    }

    // 启动异步线程
    void startAsyncWorker() {
        worker_started = true;
        worker = std::thread(&Logger::processLogs, this);
    }

    // 同步写日志
    void logSync(LogLevel level, const std::string& message,
                 const std::string& file, const std::string& function, int line) {
        std::ostringstream oss;
        oss << getCurrentTime() << " " << logLevelToString(level)
            << " " << file << ":" << line << " " << function
            << " - " << message;

        std::lock_guard<std::mutex> lock(sync_mtx);
        std::cout << oss.str() << std::endl;
        if (log_file.is_open()) {
            log_file << oss.str() << std::endl;
        }
    }

    // 异步记录日志
    void logAsync(LogLevel level, const std::string& message,
                  const std::string& file, const std::string& function, int line) {
        LogEntry entry{level, message, file, function, getCurrentTime(), line};

        {
            std::lock_guard<std::mutex> lock(buffer_mutex);
            front_buffer.push_back(std::move(entry));
        }

        cv.notify_one();
    }

    // 异步线程主循环
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
                for (const auto& e : back_buffer) {
                    writeLog(e);
                }
                back_buffer.clear();
            }
        }

        for (const auto& e : front_buffer) {
            writeLog(e);
        }
    }

    // 实际写入操作
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

// ----------- 宏定义统一接口 -------------
#define LOG_DEBUG(msg) Logger::instance().log(Logger::LogLevel::DEBUG, msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_INFO(msg) Logger::instance().log(Logger::LogLevel::INFO, msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_WARNING(msg) Logger::instance().log(Logger::LogLevel::WARNING, msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_ERROR(msg) Logger::instance().log(Logger::LogLevel::ERROR, msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_CRITICAL(msg) Logger::instance().log(Logger::LogLevel::CRITICAL, msg, __FILE__, __FUNCTION__, __LINE__)

// 测试
int main() {
    auto& logger = Logger::instance();
    logger.setLogLevel(Logger::LogLevel::DEBUG);

    std::cout << "==== 同步模式 ====" << std::endl;
    logger.setAsyncMode(false);
    LOG_INFO("This is sync mode log.");
    LOG_ERROR("Sync mode error log.");

    std::cout << "\n==== 异步模式 ====" << std::endl;
    logger.setAsyncMode(true);
    for (int i = 0; i < 5; ++i) {
        LOG_INFO("Async log " + std::to_string(i));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    LOG_CRITICAL("Final async log.");
}
