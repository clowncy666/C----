#pragma once
#include "ILogSink.h"
#include "RollingFileManager.h"
#include <memory>
#include <mutex>
#include <filesystem>

class BagSink : public ILogSink {
public:
    BagSink(const std::filesystem::path& base_dir,
           const std::string& module_name,
           const std::string& pattern,
           size_t max_bytes,
           std::chrono::minutes max_age,
           size_t reserve_n,
           bool compress_old);
    
    void writeText(const std::string&) override {
        // Bag Sink 不处理文本数据
    }
    
    void writeBinary(const std::vector<uint8_t>&, const std::string&, uint64_t) override {
        // Bag Sink 不处理二进制数据
    }
    
    void writeMessage(const std::string& topic,
                     const std::string& type,
                     const std::vector<uint8_t>& data,
                     uint64_t timestamp) override;
    
    bool needRotate() override;
    void rotate() override;
    bool ensureWritable(size_t bytes_hint) override;
    void flush() override;
    
private:
    std::unique_ptr<RollingFileManager> rolling_mgr_;
    std::mutex mtx_;
};