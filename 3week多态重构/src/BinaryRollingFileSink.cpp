#include "BinaryRollingFileSink.h"
#include <iostream>

BinaryRollingFileSink::BinaryRollingFileSink(
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

void BinaryRollingFileSink::writeBinary(
    const std::vector<uint8_t>& data,
    const std::string& tag,
    uint64_t timestamp)
{
    std::lock_guard<std::mutex> lock(mtx_);
    
    if (needRotate()) {
        rotate();
    }
    
    uint32_t tag_len = static_cast<uint32_t>(tag.size());
    uint32_t data_len = static_cast<uint32_t>(data.size());
    size_t total_size = sizeof(timestamp) + sizeof(tag_len) + tag_len + 
                       sizeof(data_len) + data_len;
    
    if (!ensureWritable(total_size)) {
        return; // 磁盘空间不足
    }
    
    auto& os = rolling_mgr_->stream();
    if (os.good()) {
        os.write(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp));
        os.write(reinterpret_cast<const char*>(&tag_len), sizeof(tag_len));
        os.write(tag.data(), tag_len);
        os.write(reinterpret_cast<const char*>(&data_len), sizeof(data_len));
        os.write(reinterpret_cast<const char*>(data.data()), data_len);
    }
}

bool BinaryRollingFileSink::needRotate() {
    return rolling_mgr_->needRotate();
}

void BinaryRollingFileSink::rotate() {
    rolling_mgr_->rotate();
}

bool BinaryRollingFileSink::ensureWritable(size_t bytes_hint) {
    return rolling_mgr_->ensureWritable(bytes_hint);
}

void BinaryRollingFileSink::flush() {
    std::lock_guard<std::mutex> lock(mtx_);
    auto& os = rolling_mgr_->stream();
    if (os.good()) {
        os.flush();
    }
}