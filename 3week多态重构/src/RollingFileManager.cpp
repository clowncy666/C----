#include "RollingFileManager.h"
#include <zlib.h>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <algorithm>

RollingFileManager::RollingFileManager(
    std::filesystem::path baseDir,
    std::string pattern,
    size_t maxBytes,
    std::chrono::minutes maxAge,
    size_t reserveN,
    bool compressOld)
    : base_dir_(ProcessUtils::getProcessLogDir(baseDir)),  // 关键：自动加上 <proc>/<pid>
      pattern_(std::move(pattern)),
      max_bytes_(maxBytes),
      max_age_(maxAge),
      reserve_n_(reserveN),
      compress_(compressOld),
      guard_(std::make_unique<DiskSpaceGuard>(
    base_dir_, "", expectedExtension(), 
    DiskPolicy{100ULL * 1024 * 1024, 50ULL * 1024 * 1024, 2}))
{
    std::error_code ec;
    std::filesystem::create_directories(base_dir_, ec);
    if (ec) {
        std::cerr << "[RollingFileManager] Failed to create directory: " 
                  << base_dir_ << " - " << ec.message() << std::endl;
    }
    
    // 尝试复用最新的可追加文件
    auto resume = findLatestAppendableFile();
    if (!resume.empty()) {
        current_path_ = resume;
        ofs_.open(current_path_, std::ios::out | std::ios::app);
        if (!ofs_.is_open()) {
            rollToNewFile();
        }
    } else {
        rollToNewFile();
    }
}

RollingFileManager::~RollingFileManager() {
    if (ofs_.is_open()) {
        ofs_.close();
    }
}

std::ofstream& RollingFileManager::stream() {
    return ofs_;
}

std::filesystem::path RollingFileManager::currentPath() const {
    return current_path_;
}

bool RollingFileManager::ensureWritable(size_t /*bytes_hint*/) {
    if (guard_.hardPressure()) {
        if (!suspend_writes_) {
            std::cerr << "[Log] Disk hard pressure; suspend writes.\n";
        }
        suspend_writes_ = true;
        return false;
    }
    
    if (!guard_.ensureSoft()) {
        std::cerr << "[Log] Disk space low; unable to ensure writable.\n";
        return false;
    }
    
    if (suspend_writes_) {
        std::cerr << "[Log] Disk pressure relieved; resume writes.\n";
    }
    suspend_writes_ = false;
    return true;
}

bool RollingFileManager::needRotate() {
    if (!ofs_.is_open()) return true;
    
    try {
        auto sz = std::filesystem::file_size(current_path_);
        if (sz >= max_bytes_) return true;
    } catch (...) {
        return true;
    }
    
    std::error_code ec;
    auto mtime = std::filesystem::last_write_time(current_path_, ec);
    if (!ec) {
        auto now_file = std::filesystem::file_time_type::clock::now();
        if (now_file - mtime >= max_age_) return true;
    } else {
        return true;
    }
    
    return false;
}

void RollingFileManager::rotate() {
    if (ofs_.is_open()) {
        ofs_.flush();
        ofs_.close();
    }
    
    if (compress_) {
        try { compressFile(current_path_); }
        catch (...) {}
    }
    
    enforceReserveN();
    rollToNewFile();
}

void RollingFileManager::enforceReserveN() {
    std::vector<std::filesystem::directory_entry> entries;
    std::error_code ec;
    
    for (auto& entry : std::filesystem::directory_iterator(base_dir_, ec)) {
        if (ec) break;
        if (entry.is_regular_file()) {
            entries.push_back(entry);
        }
    }
    
    std::sort(entries.begin(), entries.end(),
        [](const auto& a, const auto& b) {
            std::error_code e1, e2;
            return a.last_write_time(e1) > b.last_write_time(e2);
        });
    
    for (size_t i = reserve_n_; i < entries.size(); ++i) {
        std::error_code ec2;
        std::filesystem::remove(entries[i].path(), ec2);
        if (ec2) {
            std::cerr << "[RollingFileManager] Failed to remove old file: "
                      << entries[i].path() << " - " << ec2.message() << std::endl;
        }
    }
}

std::string RollingFileManager::nowStr(const char* fmt)const {
    auto t = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    std::tm tm = *std::localtime(&t);
    char buf[64]{};
    std::strftime(buf, sizeof(buf), fmt, &tm);
    return buf;
}

std::string RollingFileManager::makeFilename(int seq) const {
    std::string ts = nowStr("%Y%m%d_%H%M%S");
    auto p = pattern_;
    
    auto pos = p.find("%Y%m%d_%H%M%S");
    if (pos != std::string::npos) {
        p.replace(pos, 13, ts);
    }
    
    pos = p.find("%03d");
    if (pos != std::string::npos) {
        std::ostringstream oss;
        oss << std::setw(3) << std::setfill('0') << seq;
        p.replace(pos, 4, oss.str());
    }
    
    return p;
}

void RollingFileManager::rollToNewFile() {
    for (int i = 0; i < 1000; ++i) {
        auto candidate = base_dir_ / makeFilename(i);
        std::error_code ec;
        bool exists_txt = std::filesystem::exists(candidate, ec);
        bool exists_gz = std::filesystem::exists(candidate.string() + ".gz");
        
        if (!exists_txt && !exists_gz) {
            current_path_ = candidate;
            ofs_.open(current_path_, std::ios::out | std::ios::app);
            return;
        }
    }
    
    current_path_ = base_dir_ / makeFilename(999);
    ofs_.open(current_path_, std::ios::out | std::ios::app);
}

void RollingFileManager::compressFile(const std::filesystem::path& src) {
    std::ifstream in(src, std::ios::binary);
    if (!in) return;
    
    auto gzPath = src.string() + ".gz";
    gzFile out = gzopen(gzPath.c_str(), "wb");
    if (!out) return;
    
    char buffer[1 << 16];
    while (in) {
        in.read(buffer, sizeof(buffer));
        auto n = in.gcount();
        if (n > 0) {
            gzwrite(out, buffer, static_cast<unsigned int>(n));
        }
    }
    
    gzclose(out);
    in.close();
    
    std::error_code ec;
    std::filesystem::remove(src, ec);
}

std::string RollingFileManager::expectedExtension() const {
    auto pos = pattern_.find_last_of('.');
    if (pos == std::string::npos) return {};
    return pattern_.substr(pos);
}

std::filesystem::path RollingFileManager::findLatestAppendableFile() const {
    std::error_code ec;
    std::vector<std::filesystem::directory_entry> files;
    const auto wantExt = expectedExtension();
    
    for (auto& e : std::filesystem::directory_iterator(base_dir_, ec)) {
        if (ec) break;
        if (!e.is_regular_file()) continue;
        
        auto p = e.path();
        if (p.extension() == ".gz") continue;
        if (!wantExt.empty() && p.extension() != wantExt) continue;
        
        files.push_back(e);
    }
    
    if (files.empty()) return {};
    
    std::sort(files.begin(), files.end(), [](auto& a, auto& b) {
        std::error_code e1, e2;
        return a.last_write_time(e1) > b.last_write_time(e2);
    });
    
    auto candidate = files.front().path();
    
    try {
        auto sz = std::filesystem::file_size(candidate);
        if (sz >= max_bytes_) return {};
        
        std::error_code ec2;
        auto mtime = std::filesystem::last_write_time(candidate, ec2);
        if (ec2) return {};
        
        auto now_file = std::filesystem::file_time_type::clock::now();
        if (now_file - mtime >= max_age_) return {};
        
        return candidate;
    } catch (...) {
        return {};
    }
}