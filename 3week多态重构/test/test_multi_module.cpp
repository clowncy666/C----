#include "LoggerCore.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

int main() {
    auto& logger = LoggerCore::instance();
    
    // 初始化 Sinks
    logger.initSinks("./logs");
    logger.setLogLevel(LogLevel::DEBUG);
    
    std::cout << "====== 测试1：同步模式 - 多模块写入 ======\n";
    logger.setAsyncMode(false);
    
    // 写文本日志（应写入 text/ 目录）
    LOG_INFO("Sync text log - module=text");
    LOG_WARNING("Another sync text log");
    
    // 写二进制日志（应写入 binary/ 目录）
    std::vector<uint8_t> bin_data = {0x01, 0x02, 0x03, 0x04, 0xAA, 0xBB};
    logger.logBinary(bin_data.data(), bin_data.size(), "sensor_data");
    
    // 写消息记录（应写入 bag/ 目录）
    std::vector<uint8_t> msg_data = {0x10, 0x20, 0x30};
    logger.recordMessage("/camera/image", "sensor_msgs/Image", msg_data);
    
    std::cout << "\n====== 测试2：切换到异步模式 ======\n";
    logger.setAsyncMode(true);
    
    // 异步写入混合日志
    for (int i = 0; i < 100; ++i) {
        LOG_INFO("Async text log #" + std::to_string(i));
        
        if (i % 10 == 0) {
            std::vector<uint8_t> data(128);
            std::fill(data.begin(), data.end(), i % 256);
            logger.logBinary(data.data(), data.size(), "batch_" + std::to_string(i));
        }
        
        if (i % 15 == 0) {
            std::vector<uint8_t> msg(64, i % 256);
            logger.recordMessage("/lidar/scan", "sensor_msgs/LaserScan", msg);
        }
    }
    
    std::cout << "\n====== 测试3：批量写入触发轮转 ======\n";
    
    // 文本大批量
    std::string payload(1024, 'T');
    for (int i = 0; i < 1500; ++i) {
        LOG_INFO("Bulk text #" + std::to_string(i) + " " + payload);
        if (i % 100 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // 二进制大批量
    std::vector<uint8_t> large_bin(4096, 0xBB);
    for (int i = 0; i < 1200; ++i) {
        logger.logBinary(large_bin.data(), large_bin.size(), "large_" + std::to_string(i));
        if (i % 50 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // 消息大批量
    std::vector<uint8_t> large_msg(8192, 0xCC);
    for (int i = 0; i < 1000; ++i) {
        logger.recordMessage("/camera/raw", "sensor_msgs/CompressedImage", large_msg);
        if (i % 30 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    std::cout << "\n====== 测试4：混合并发写入 ======\n";
    
    std::vector<std::thread> threads;
    
    threads.emplace_back([&]() {
        for (int i = 0; i < 300; ++i) {
            LOG_DEBUG("Thread1 text #" + std::to_string(i));
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });
    
    threads.emplace_back([&]() {
        for (int i = 0; i < 200; ++i) {
            std::vector<uint8_t> data(512, i % 256);
            logger.logBinary(data.data(), data.size(), "thread2_bin");
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
        }
    });
    
    threads.emplace_back([&]() {
        for (int i = 0; i < 150; ++i) {
            std::vector<uint8_t> msg(256, i % 256);
            logger.recordMessage("/imu/data", "sensor_msgs/Imu", msg);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
    
    for (auto& t : threads) {
        t.join();
    }
    
    // 给异步队列时间刷盘
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    std::cout << "\n====== 验收结果 ======\n";
    std::cout << "请检查目录结构：\n";
    std::cout << "  ./logs/<proc_name>/<pid>/text/    - 文本日志\n";
    std::cout << "  ./logs/<proc_name>/<pid>/binary/  - 二进制日志\n";
    std::cout << "  ./logs/<proc_name>/<pid>/bag/     - 消息包\n";
    
    return 0;
}