#pragma once
#include "../core/ILogSink.h"
#include "../manager/RollingFileManager.h" 
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
    ~TextRollingFileSink() override;
    void writeText(const std::string& formatted_message) override;
    
    void writeBinary(const std::vector<uint8_t>&, const std::string&, uint64_t) override {
        // 文本 Sink 不处理二进制数据
    }
    
    void writeMessage(const std::string&, const std::string&, 
                     const std::vector<uint8_t>&, uint64_t) override {
        // 文本 Sink 不处理消息数据
    }
    void flush() override;
    
protected:
    bool needRotate() override;
    void rotate() override;
    bool ensureWritable(size_t bytes_hint) override;
    
private:
    // 改进：使用 unique_ptr 明确所有权
    std::unique_ptr<RollingFileManager> rolling_mgr_;
    
    // 改进：使用递归互斥锁，防止死锁
    mutable std::recursive_mutex mtx_;
    size_t total_writes_{0};
    size_t total_bytes_{0};
};