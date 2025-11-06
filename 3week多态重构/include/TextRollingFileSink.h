#pragma once
#include "ILogSink.h"
#include "RollingFileManager.h"
#include <memory>
#include <mutex>
#include <filesystem>

class TextRollingFileSink : public ILogSink {
public:
    TextRollingFileSink(const std::filesystem::path& base_dir,
                       const std::string& module_name,
                       const std::string& pattern,
                       size_t max_bytes,
                       std::chrono::minutes max_age,
                       size_t reserve_n,
                       bool compress_old);
    
    void writeText(const std::string& formatted_message) override;
    
    void writeBinary(const std::vector<uint8_t>&, const std::string&, uint64_t) override {
        // 文本 Sink 不处理二进制数据
    }
    
    void writeMessage(const std::string&, const std::string&, 
                     const std::vector<uint8_t>&, uint64_t) override {
        // 文本 Sink 不处理消息数据
    }
    
    bool needRotate() override;
    void rotate() override;
    bool ensureWritable(size_t bytes_hint) override;
    void flush() override;
    
private:
    std::unique_ptr<RollingFileManager> rolling_mgr_;
    std::mutex mtx_;
};