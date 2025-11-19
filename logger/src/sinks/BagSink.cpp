#include "BagSink.h"
#include <iostream>

BagSink::BagSink(
    const std::filesystem::path& base_dir,
    const std::string& module_name,
    const std::string& pattern,
    size_t max_bytes,
    std::chrono::minutes max_age,
    size_t reserve_n,
    bool compress_old)
{
    auto module_dir = base_dir / module_name;
    
    rolling_mgr_ = std::make_unique<RollingFileManager>(
        module_dir, pattern, max_bytes, max_age, reserve_n, compress_old
    );
}

void BagSink::writeMessage(
    const std::string& topic,
    const std::string& type,
    const std::vector<uint8_t>& data,
    uint64_t timestamp)
{
    std::lock_guard<std::mutex> lock(mtx_);
    
    if (needRotate()) {
        rotate();
    }
    
    uint32_t topic_len = static_cast<uint32_t>(topic.size());
    uint32_t type_len = static_cast<uint32_t>(type.size());
    uint32_t data_len = static_cast<uint32_t>(data.size());
    
    size_t total_size = sizeof(timestamp) + 
                       sizeof(topic_len) + topic_len +
                       sizeof(type_len) + type_len +
                       sizeof(data_len) + data_len;
    
    if (!ensureWritable(total_size)) {
        return; // 磁盘空间不足
    }
    
    auto& os = rolling_mgr_->stream();
    if (os.good()) {
        os.write(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp));
        os.write(reinterpret_cast<const char*>(&topic_len), sizeof(topic_len));
        os.write(topic.data(), topic_len);
        os.write(reinterpret_cast<const char*>(&type_len), sizeof(type_len));
        os.write(type.data(), type_len);
        os.write(reinterpret_cast<const char*>(&data_len), sizeof(data_len));
        os.write(reinterpret_cast<const char*>(data.data()), data_len);
    }
}

bool BagSink::needRotate() {
    return rolling_mgr_->needRotate();
}

void BagSink::rotate() {
    rolling_mgr_->rotate();
}

bool BagSink::ensureWritable(size_t bytes_hint) {
    return rolling_mgr_->ensureWritable(bytes_hint);
}

void BagSink::flush() {
    std::lock_guard<std::mutex> lock(mtx_);
    auto& os = rolling_mgr_->stream();
    if (os.good()) {
        os.flush();
    }
}