#include "TextRollingFileSink.h"
#include <iostream>

TextRollingFileSink::TextRollingFileSink(
    const std::filesystem::path& base_dir,
    const std::string& module_name,
    const std::string& pattern,
    size_t max_bytes,
    std::chrono::minutes max_age,
    size_t reserve_n,
    bool compress_old)
{
    // 拼接模块子目录：<base>/<proc_name>/<pid>/<module>/
    auto module_dir = base_dir / module_name;
    
    rolling_mgr_ = std::make_unique<RollingFileManager>(
        module_dir, pattern, max_bytes, max_age, reserve_n, compress_old
    );
}

void TextRollingFileSink::writeText(const std::string& formatted_message) {
    std::lock_guard<std::mutex> lock(mtx_);
    
    if (needRotate()) {
        rotate();
    }
    
    if (!ensureWritable(formatted_message.size() + 128)) {
        return; // 磁盘空间不足，跳过写入
    }
    
    auto& os = rolling_mgr_->stream();
    if (os.good()) {
        os << formatted_message << std::endl;
    }
}

bool TextRollingFileSink::needRotate() {
    return rolling_mgr_->needRotate();
}

void TextRollingFileSink::rotate() {
    rolling_mgr_->rotate();
}

bool TextRollingFileSink::ensureWritable(size_t bytes_hint) {
    return rolling_mgr_->ensureWritable(bytes_hint);
}

void TextRollingFileSink::flush() {
    std::lock_guard<std::mutex> lock(mtx_);
    auto& os = rolling_mgr_->stream();
    if (os.good()) {
        os.flush();
    }
}