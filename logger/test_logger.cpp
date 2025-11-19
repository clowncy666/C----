/**
 * @file test_logger.cpp
 * @brief 日志系统功能验证程序
 */

#include <logger/Logger.h>
#include <iostream>
#include <cassert>
#include <filesystem>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

// 测试计数器
int passed = 0;
int failed = 0;

#define TEST_CASE(name) \
    std::cout << "\n🧪 测试: " << name << std::endl

#define TEST_ASSERT(condition, msg) \
    do { \
        if (condition) { \
            std::cout << "  ✅ " << msg << std::endl; \
            passed++; \
        } else { \
            std::cout << "  ❌ " << msg << " (FAILED)" << std::endl; \
            failed++; \
        } \
    } while(0)

// 清理测试目录
void cleanupTestDir(const std::string& dir) {
    if (fs::exists(dir)) {
        fs::remove_all(dir);
    }
}

// 检查日志文件是否存在
bool logFileExists(const std::string& base_dir, const std::string& pattern) {
    if (!fs::exists(base_dir)) return false;
    
    for (const auto& entry : fs::recursive_directory_iterator(base_dir)) {
        if (entry.is_regular_file()) {
            return true;
        }
    }
    return false;
}

// ============================================
// 测试1: 基本初始化和日志输出
// ============================================
void test_basic_logging() {
    TEST_CASE("基本日志功能");
    
    cleanupTestDir("./test_logs");
    
    // 创建配置
    LoggerConfig config;
    config.base_dir = "./test_logs";
    config.log_level = LogLevel::DEBUG;
    config.async_mode = false;  // 同步模式便于测试
    
    config.modules.push_back(ModuleConfig{
        "text", "test_%Y%m%d_%H%M%S_%03d.log",
        1024 * 1024, std::chrono::minutes(60), 3, false
    });
    
    logger::Logger::instance().init(config);
    
    // 写入日志
    LOG_DEBUG("Debug message");
    LOG_INFO("Info message");
    LOG_WARNING("Warning message");
    LOG_ERROR("Error message");
    LOG_CRITICAL("Critical message");
    
    // 格式化日志
    LOG_INFO_FMT("Formatted: %d + %d = %d", 1, 2, 3);
    
    // 验证日志文件存在
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    TEST_ASSERT(logFileExists("./test_logs", "test_*.log"), 
                "日志文件已创建");
}

// ============================================
// 测试2: 日志级别过滤
// ============================================
void test_log_level_filter() {
    TEST_CASE("日志级别过滤");
    
    cleanupTestDir("./test_logs2");
    
    LoggerConfig config;
    config.base_dir = "./test_logs2";
    config.log_level = LogLevel::WARNING;  // 只记录 WARNING 及以上
    config.async_mode = false;
    
    config.modules.push_back(ModuleConfig{
        "text", "level_test_%Y%m%d.log",
        1024 * 1024, std::chrono::minutes(60), 3, false
    });
    
    logger::Logger::instance().init(config);
    
    LOG_DEBUG("Should NOT appear");
    LOG_INFO("Should NOT appear");
    LOG_WARNING("Should appear");
    LOG_ERROR("Should appear");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    TEST_ASSERT(logFileExists("./test_logs2", "level_test_*.log"), 
                "级别过滤生效");
}

// ============================================
// 测试3: 异步模式
// ============================================
void test_async_mode() {
    TEST_CASE("异步模式");
    
    cleanupTestDir("./test_logs3");
    
    LoggerConfig config;
    config.base_dir = "./test_logs3";
    config.log_level = LogLevel::INFO;
    config.async_mode = true;  // 异步模式
    config.async_queue_size = 1000;
    
    config.modules.push_back(ModuleConfig{
        "text", "async_test_%Y%m%d.log",
        1024 * 1024, std::chrono::minutes(60), 3, false
    });
    
    logger::Logger::instance().init(config);
    
    // 快速写入大量日志
    for (int i = 0; i < 100; ++i) {
        LOG_INFO_FMT("Async log %d", i);
    }
    
    // 等待异步队列处理
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    TEST_ASSERT(logFileExists("./test_logs3", "async_test_*.log"), 
                "异步模式正常工作");
}

// ============================================
// 测试4: 二进制日志
// ============================================
void test_binary_logging() {
    TEST_CASE("二进制日志");
    
    cleanupTestDir("./test_logs4");
    
    LoggerConfig config;
    config.base_dir = "./test_logs4";
    config.log_level = LogLevel::INFO;
    config.async_mode = false;
    
    config.modules.push_back(ModuleConfig{
        "binary", "binary_%Y%m%d.bin",
        1024 * 1024, std::chrono::minutes(60), 3, false
    });
    
    logger::Logger::instance().init(config);
    
    // 写入二进制数据
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    logger::Logger::instance().binary(data, sizeof(data), "test_sensor");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    TEST_ASSERT(logFileExists("./test_logs4", "binary_*.bin"), 
                "二进制日志已创建");
}

// ============================================
// 测试5: 多模块
// ============================================
void test_multi_module() {
    TEST_CASE("多模块支持");
    
    cleanupTestDir("./test_logs5");
    
    LoggerConfig config;
    config.base_dir = "./test_logs5";
    config.log_level = LogLevel::INFO;
    config.async_mode = false;
    
    // 添加多个模块
    config.modules.push_back(ModuleConfig{
        "text", "app_%Y%m%d.log",
        1024 * 1024, std::chrono::minutes(60), 3, false
    });
    
    config.modules.push_back(ModuleConfig{
        "binary", "data_%Y%m%d.bin",
        1024 * 1024, std::chrono::minutes(60), 3, false
    });
    
    config.modules.push_back(ModuleConfig{
        "bag", "msg_%Y%m%d.bag",
        1024 * 1024, std::chrono::minutes(60), 3, false
    });
    
    logger::Logger::instance().init(config);
    
    // 写入不同类型的日志
    LOG_INFO("Text log");
    
    uint8_t data[] = {0xAA, 0xBB};
    logger::Logger::instance().binary(data, sizeof(data), "sensor");
    
    std::vector<uint8_t> msg_data = {0x01, 0x02};
    logger::Logger::instance().message("/test/topic", "TestType", msg_data);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    TEST_ASSERT(logFileExists("./test_logs5", "app_*.log"), 
                "文本模块正常");
    TEST_ASSERT(logFileExists("./test_logs5", "data_*.bin"), 
                "二进制模块正常");
    TEST_ASSERT(logFileExists("./test_logs5", "msg_*.bag"), 
                "消息模块正常");
}

// ============================================
// 测试6: 配置重载
// ============================================
void test_config_reload() {
    TEST_CASE("配置重载");
    
    cleanupTestDir("./test_logs6");
    
    // 创建配置文件
    const char* config_content = R"({
        "base_dir": "./test_logs6",
        "log_level": "INFO",
        "async_mode": false,
        "modules": [{
            "name": "text",
            "pattern": "reload_%Y%m%d.log",
            "max_bytes_mb": 1,
            "max_age_minutes": 60,
            "reserve_n": 3,
            "compress_old": false
        }]
    })";
    
    std::ofstream ofs("test_config.json");
    ofs << config_content;
    ofs.close();
    
    // 从文件初始化
    logger::Logger::instance().init("test_config.json");
    
    LOG_INFO("Initial log");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    TEST_ASSERT(logFileExists("./test_logs6", "reload_*.log"), 
                "配置文件加载成功");
    
    // 清理
    fs::remove("test_config.json");
}

// ============================================
// 测试7: 运行时级别调整
// ============================================
void test_runtime_level_change() {
    TEST_CASE("运行时级别调整");
    
    cleanupTestDir("./test_logs7");
    
    LoggerConfig config;
    config.base_dir = "./test_logs7";
    config.log_level = LogLevel::INFO;
    config.async_mode = false;
    
    config.modules.push_back(ModuleConfig{
        "text", "runtime_%Y%m%d.log",
        1024 * 1024, std::chrono::minutes(60), 3, false
    });
    
    logger::Logger::instance().init(config);
    
    LOG_DEBUG("Should NOT appear 1");
    LOG_INFO("Should appear 1");
    
    // 运行时调整级别
    logger::Logger::instance().setLevel(LogLevel::DEBUG);
    
    LOG_DEBUG("Should appear 2");
    LOG_INFO("Should appear 3");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    TEST_ASSERT(logFileExists("./test_logs7", "runtime_*.log"), 
                "运行时级别调整生效");
}

// ============================================
// 测试8: 性能压力测试
// ============================================
void test_performance() {
    TEST_CASE("性能测试");
    
    cleanupTestDir("./test_logs_perf");
    
    LoggerConfig config;
    config.base_dir = "./test_logs_perf";
    config.log_level = LogLevel::INFO;
    config.async_mode = true;
    config.async_queue_size = 50000;
    
    config.modules.push_back(ModuleConfig{
        "text", "perf_%Y%m%d.log",
        10 * 1024 * 1024, std::chrono::minutes(60), 3, false
    });
    
    logger::Logger::instance().init(config);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // 写入10000条日志
    for (int i = 0; i < 10000; ++i) {
        LOG_INFO_FMT("Performance test log %d", i);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double logs_per_sec = 10000.0 * 1000.0 / duration.count();
    
    std::cout << "  📊 写入10000条日志耗时: " << duration.count() << " ms" << std::endl;
    std::cout << "  📊 吞吐量: " << static_cast<int>(logs_per_sec) << " logs/sec" << std::endl;
    
    // 等待异步队列清空
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    TEST_ASSERT(logFileExists("./test_logs_perf", "perf_*.log"), 
                "性能测试通过");
}

// ============================================
// 主函数
// ============================================
int main() {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "    Logger 功能验证测试\n";
    std::cout << "========================================\n";
    
    try {
        test_basic_logging();
        test_log_level_filter();
        test_async_mode();
        test_binary_logging();
        test_multi_module();
        test_config_reload();
        test_runtime_level_change();
        test_performance();
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ 测试异常: " << e.what() << std::endl;
        return 1;
    }
    
    // 清理测试目录
    std::cout << "\n🧹 清理测试目录..." << std::endl;
    cleanupTestDir("./test_logs");
    cleanupTestDir("./test_logs2");
    cleanupTestDir("./test_logs3");
    cleanupTestDir("./test_logs4");
    cleanupTestDir("./test_logs5");
    cleanupTestDir("./test_logs6");
    cleanupTestDir("./test_logs7");
    cleanupTestDir("./test_logs_perf");
    
    // 输出测试结果
    std::cout << "\n========================================\n";
    std::cout << "    测试结果\n";
    std::cout << "========================================\n";
    std::cout << "✅ 通过: " << passed << std::endl;
    std::cout << "❌ 失败: " << failed << std::endl;
    std::cout << "========================================\n\n";
    
    if (failed > 0) {
        std::cout << "❌ 部分测试失败！\n" << std::endl;
        return 1;
    } else {
        std::cout << "🎉 所有测试通过！\n" << std::endl;
        return 0;
    }
}