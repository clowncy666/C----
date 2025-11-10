#pragma once
#include <string>
#include <vector>
#include <cstdint>

// 日志输出接口
class ILogSink {
public:
    virtual ~ILogSink() = default;
    
    // 写入文本日志
    virtual void writeText(const std::string& formatted_message) = 0;
    
    // 写入二进制数据
    virtual void writeBinary(const std::vector<uint8_t>& data, 
                            const std::string& tag, 
                            uint64_t timestamp) = 0;
    
    // 写入消息记录
    virtual void writeMessage(const std::string& topic,
                             const std::string& type,
                             const std::vector<uint8_t>& data,
                             uint64_t timestamp) = 0;
    // 刷新缓冲
    virtual void flush() = 0;
protected:
    // 检查是否需要轮转
    virtual bool needRotate() = 0;
    
    // 执行轮转
    virtual void rotate() = 0;
    
    // 确保可写（磁盘空间检查）
    virtual bool ensureWritable(size_t bytes_hint) = 0;
};
class ITextSink {
public:
    virtual ~ITextSink() = default;
    virtual void writeText(const std::string& formatted_message) = 0;
    virtual void flush() = 0;
};

// 只处理二进制的 Sink
class IBinarySink {
public:
    virtual ~IBinarySink() = default;
    virtual void writeBinary(const std::vector<uint8_t>& data, 
                            const std::string& tag, 
                            uint64_t timestamp) = 0;
    virtual void flush() = 0;
};

// 只处理消息的 Sink
class IMessageSink {
public:
    virtual ~IMessageSink() = default;
    virtual void writeMessage(const std::string& topic,
                             const std::string& type,
                             const std::vector<uint8_t>& data,
                             uint64_t timestamp) = 0;
    virtual void flush() = 0;
};