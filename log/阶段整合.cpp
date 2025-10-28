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
#include <zlib.h> // 包含了 zlib 库的函数，我们用它来压缩


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

    // 二进制日志接口
    void logBinary(const void* data, size_t size, const std::string& tag = "binary") {
        if (async_mode) {
            logBinaryAsync(data, size, tag);
        } else {
            logBinarySync(data, size, tag);
        }
    }

    // 消息记录
    void recordMessage(const std::string& topic, const std::string& type, const std::vector<uint8_t>& data) {
        MessageRecord record{topic, type, data, static_cast<uint64_t>(std::time(nullptr))};
        
        if (async_mode) {
            recordMessageAsync(record);
        } else {
            recordMessageSync(record);
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
        if (binary_file.is_open()) {
            binary_file.close();
        }
        if (bag_file.is_open()) {
            bag_file.close();
        }

    }

private:
    struct LogEntry {
        LogLevel level;
        std::string message, file, function, timestamp;
        int line;
    };
    struct BinaryEntry {
        std::vector<uint8_t> data;
        std::string tag;
        uint64_t timestamp;
    };

    struct MessageRecord {
        std::string topic;       // 主题
        std::string type;        // 消息类型
        std::vector<uint8_t> data;  // 消息内容（二进制数据）
        uint64_t timestamp;      // 时间戳
    };

    LogLevel current_level = LogLevel::INFO;
    bool async_mode = false;
    std::atomic<bool> stop{false};
    bool worker_started = false;

    // 同步部分
    std::mutex sync_mtx;
    std::mutex buffer_mutex;
    std::mutex binary_mtx;

    // 异步部分
    std::vector<LogEntry> front_buffer, back_buffer;
    std::vector<MessageRecord> front_message_buffer, back_message_buffer;
    std::vector<BinaryEntry> binary_front, binary_back;
    std::thread worker;
    std::condition_variable cv;
    std::ofstream log_file, binary_file, bag_file;

    size_t max_file_size = 10 *1024 * 1024; 
    size_t max_time_minutes =60; 
    std::string current_log_file="log_";
    std::string current_bag_file="messages_bag_";
    std::ofstream current_log;
    std::ofstream current_bag;
    std::time_t last_rotation_time = std::time(nullptr);

    Logger() { 
        openLogFile();
        openBinaryFile();
     
    }

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

    // 打开日志文件
    void openLogFile() {
        log_file.open("log.txt", std::ios_base::app);
        if (!log_file.is_open()) {
            std::cerr << "Failed to open log file!" << std::endl;
        }
    }

    // 打开二进制日志文件
    void openBinaryFile() {
        binary_file.open("binary_log.bin", std::ios::binary | std::ios::app);
        if (!binary_file.is_open()) {
            std::cerr << "Failed to open binary log file!" << std::endl;
        }
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
        rotateLogFileIfNeeded(); 
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

    // 同步写二进制日志
    void logBinarySync(const void* data, size_t size, const std::string& tag) {
        std::lock_guard<std::mutex> lock(binary_mtx);
        if (binary_file.is_open()) {
            auto now = std::chrono::system_clock::now();
            uint64_t ts = std::chrono::duration_cast<std::chrono::microseconds>(
                now.time_since_epoch()).count();

            uint32_t tag_len = static_cast<uint32_t>(tag.size());
            uint32_t data_len = static_cast<uint32_t>(size);

            binary_file.write(reinterpret_cast<const char*>(&ts), sizeof(ts));
            binary_file.write(reinterpret_cast<const char*>(&tag_len), sizeof(tag_len));
            binary_file.write(tag.data(), tag_len);
            binary_file.write(reinterpret_cast<const char*>(&data_len), sizeof(data_len));
            binary_file.write(reinterpret_cast<const char*>(data), data_len);
        }
    }

    // 异步记录二进制日志
    void logBinaryAsync(const void* data, size_t size, const std::string& tag) {
        BinaryEntry entry;
        entry.data.assign((uint8_t*)data, (uint8_t*)data + size);
        entry.tag = tag;
        entry.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        {
            std::lock_guard<std::mutex> lock(buffer_mutex);
            binary_front.push_back(std::move(entry));
        }
        cv.notify_one();
    }


    // 写入消息日志
    void writeMessage(const MessageRecord& record) {
        std::lock_guard<std::mutex> lock(sync_mtx);

        if (!bag_file.is_open()) {
            bag_file.open("messages.bag", std::ios::binary | std::ios::app);
            if (!bag_file.is_open()) {
                std::cerr << "Failed to open bag file!" << std::endl;
                return;
            }
        }

        uint64_t timestamp = record.timestamp;
        uint32_t topic_len = record.topic.size();
        uint32_t type_len = record.type.size();
        uint32_t data_len = record.data.size();

        bag_file.write(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp));
        bag_file.write(reinterpret_cast<const char*>(&topic_len), sizeof(topic_len));
        bag_file.write(record.topic.data(), topic_len);
        bag_file.write(reinterpret_cast<const char*>(&type_len), sizeof(type_len));
        bag_file.write(record.type.data(), type_len);
        bag_file.write(reinterpret_cast<const char*>(&data_len), sizeof(data_len));
        bag_file.write(reinterpret_cast<const char*>(record.data.data()), data_len);
    }

    // 异步写消息
    void recordMessageAsync(const MessageRecord& record) {
        std::lock_guard<std::mutex> lock(buffer_mutex);
        front_message_buffer.push_back(record);
        cv.notify_one();
    }

    // 同步写消息
    void recordMessageSync(const MessageRecord& record) {
        writeMessage(record);
    }

    // 启动异步线程
    void startAsyncWorker() {
        worker_started = true;
        worker = std::thread(&Logger::processLogs, this);
    }

    // 异步线程主循环
    void processLogs() {
        while (!stop) {
            {
                std::unique_lock<std::mutex> lock(buffer_mutex);
                cv.wait_for(lock, std::chrono::milliseconds(500), [this] {
                    return !front_buffer.empty() || !front_message_buffer.empty() || stop;
                });
                front_buffer.swap(back_buffer);
                front_message_buffer.swap(back_message_buffer);
            }

            if (!back_buffer.empty()) {
                for (const auto& e : back_buffer) {
                    writeLog(e);
                }
                back_buffer.clear();
            }

            if (!back_message_buffer.empty()) {
                for (const auto& e : back_message_buffer) {
                    writeMessage(e);
                }
                back_message_buffer.clear();
            }
        }

        for (const auto& e : front_buffer) {
            writeLog(e);
        }

        for (const auto& e : front_message_buffer) {
            writeMessage(e);
        }
    }

    // 写入普通日志
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
    bool compressLogFile(const std::string& src, const std::string& dest) {
        //实现日志压缩功能
        gzfile* out = gzopen(dest.c_str(), "wb");
        std::ifstream in(src,std::ios_base::binary);
        char buf[4096];
        while(in.read(buf,sizeof(buf))){
            gzwrite(out,buf,in.gcount());
        }
        gzwrite(out,buf,in.gcount());
        gzclose(out);
        in.close(); 
        return true;
    }
    void createNewLogFile()
    {
        std::ostring file_name;
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&t);
        file_name << "log_" << std::put_time(&tm, "%Y%m%d_%H%M%S") <<".txt";
        current_log.open(file_name.str(),std::ios_base::app);
        if(!current_log.is_open()){
            std::cerr<<"Failed to create new log file!"<<std::endl;
        }
    }    
    void rotateLogFileIfNeeded() {
        //检查文件大小
        if(current_log.tellp() >=max_file_size){
            current_log.close();
            compressLogFile("log.txt","log.txt.gz");
            createNewLogFile();
        }
        //检查时间间隔
        std::time_t current_time =std::time(nullptr);
        double time_diff = std::difftime(current_time,last_rotation_time);
        if(time_diff >= max_time_minutes *60){
            current_log.close();
            compressLogFile("log.txt","log.txt.gz");
            createNewLogFile();
            last_rotation_time = current_time;
        }
       
        
    }
};

// ----------- 宏定义统一接口 -------------
#define LOG_DEBUG(msg) Logger::instance().log(Logger::LogLevel::DEBUG, msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_INFO(msg) Logger::instance().log(Logger::LogLevel::INFO, msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_WARNING(msg) Logger::instance().log(Logger::LogLevel::WARNING, msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_ERROR(msg) Logger::instance().log(Logger::LogLevel::ERROR, msg, __FILE__, __FUNCTION__, __LINE__)
#define LOG_CRITICAL(msg) Logger::instance().log(Logger::LogLevel::CRITICAL, msg, __FILE__, __FUNCTION__, __LINE__)



// 测试代码
int main() {
    auto& logger = Logger::instance();
    logger.setLogLevel(Logger::LogLevel::DEBUG);

    std::cout << "==== tongbu====" << std::endl;
    logger.setAsyncMode(false);
    LOG_INFO("This is sync mode log.");
    LOG_ERROR("Sync mode error log.");

    std::cout << "\n====yibu ====" << std::endl;
    logger.setAsyncMode(true);
    for (int i = 0; i < 5; ++i) {
        LOG_INFO("Async log " + std::to_string(i));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    LOG_CRITICAL("Final async log.");

    // 测试记录传感器数据
    struct SensorData {
        double temperature;
        double humidity;
        uint64_t timestamp;
    };
    SensorData s{25.6, 0.55, static_cast<uint64_t>(std::time(nullptr))};
    std::vector<uint8_t> data(reinterpret_cast<uint8_t*>(&s), reinterpret_cast<uint8_t*>(&s) + sizeof(s));
    logger.recordMessage("sensor_data", "SensorData", data);

    // 测试异步记录
    logger.setAsyncMode(true);
    for (int i = 0; i < 5; ++i) {
        logger.recordMessage("sensor_data", "SensorData", data);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Messages recorded!" << std::endl;
    return 0;
}
