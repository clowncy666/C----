#include "RollingFileManager.h"
#include <zlib.h>
#include <sstream>
#include <iostream>
#include "DiskSpaceGuard.h"

RollingFileManager::RollingFileManager(std::filesystem::path baseDir,
                                       std::string pattern,
                                       size_t maxBytes,
                                       std::chrono::minutes maxAge,
                                       size_t reserveN,
                                       bool compressOld)
    : base_dir_(std::move(baseDir)),
      pattern_(std::move(pattern)),
      max_bytes_(maxBytes),
      max_age_(maxAge),
      reserve_n_(reserveN),
      compress_(compressOld) {
    std::error_code ec;
    std::filesystem::create_directories(base_dir_, ec);
    if (ec) {
        std::cerr << "Failed to create base directory: " << ec.message() << std::endl;
    }

    // ✅ 关键改动：尝试复用最近的未压缩日志文件（若仍未超 size/age 就 append）
    auto resume = findLatestAppendableFile();
    if (!resume.empty()) {
        current_path_ = resume;
        ofs_.open(current_path_, std::ios::out | std::ios::app); // 继续追加
        if (!ofs_.is_open()) {
            // 兜底：打开失败就新建
            rollToNewFile();
        }
    } else {
        // 没有可复用的，或者已超阈值 → 新建
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
    // 如果需要写入数据大小（bytes_hint），可以基于这个值做决策
    // 但现在我们只是确保磁盘空间足够
    if (guard_.hardPressure()) {
        if (!suspend_writes_) {
            std::cerr << "[Log] Disk hard pressure; suspend file writes. "
                         "Console logging only.\n";
        }
        suspend_writes_ = true;
        return false; // 不写入
    }

    // 确保空间足够（保证磁盘可写，清理空间直到满足 soft_min_free_bytes）
    if (!guard_.ensureSoft()) {
        std::cerr << "[Log] Disk space low; unable to ensure writable space.\n";
        return false;
    }

    if (suspend_writes_) {
        std::cerr << "[Log] Disk pressure relieved; resume file writes.\n";
    }
    suspend_writes_ = false;
    return true; // 确保可以写入
}



bool RollingFileManager::needRotate() {
    if (!ofs_.is_open()) {
        return true;
    }
    try {
        auto sz = std::filesystem::file_size(current_path_);
        if (sz >= max_bytes_) {
            return true;
        }
    } catch (...) {
        return true;
    }

    //时间判断：基于文件的 mtime（跨进程有效）
    std::error_code ec;
    auto mtime = std::filesystem::last_write_time(current_path_, ec);
    if (!ec) {
        auto now_file = std::filesystem::file_time_type::clock::now();
        if (now_file - mtime >= max_age_) {
            return true;
        }
    } else {
        // 拿不到 mtime，稳妥轮转
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
        try { gzipFile(current_path_); }
        catch (...) {}
    }
    enforceReserveN();
    rollToNewFile();
}

void RollingFileManager::enforceReserveN() {
    std::vector<std::filesystem::directory_entry> entries;
    for (auto& entry : std::filesystem::directory_iterator(base_dir_)) {
        if (entry.is_regular_file()) {
            entries.push_back(entry);
        }
    }
    std::sort(entries.begin(), entries.end(),
        [](const std::filesystem::directory_entry& a,
           const std::filesystem::directory_entry& b) {
            return a.last_write_time() > b.last_write_time();
        });

    for (size_t i = reserve_n_; i < entries.size(); ++i) {
        std::error_code ec;
        std::filesystem::remove(entries[i].path(), ec);
        if (ec) {
            std::cerr << "Failed to remove old log file "
                      << entries[i].path() << ": " << ec.message() << std::endl;
        }
    }
}

std::string RollingFileManager::nowStr(const char* fmt) {
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
        bool exists_gz  = std::filesystem::exists(candidate.string() + ".gz");
        if (!exists_txt && !exists_gz) {
            current_path_ = candidate;
            ofs_.open(current_path_, std::ios::out | std::ios::app);
            // open_time_ 若存在，这里可以设置为 now；但 needRotate 已基于 mtime，无强依赖
            // open_time_ = std::chrono::system_clock::now();
            return;
        }
    }
    current_path_ = base_dir_ / makeFilename(999);
    ofs_.open(current_path_, std::ios::out | std::ios::app);
    // open_time_ = std::chrono::system_clock::now();
}

void RollingFileManager::gzipFile(const std::filesystem::path& src) {
    std::ifstream in(src, std::ios::binary);
    if (!in) { return; }
    auto gzPath = src.string() + ".gz";
    gzFile out = gzopen(gzPath.c_str(), "wb");
    if (!out) { return; }
    char buffer[1 << 16];
    while (in) {
        in.read(buffer, sizeof(buffer));
        auto n = in.gcount();
        if (n > 0) { gzwrite(out, buffer, static_cast<unsigned int>(n)); }
    }
    gzclose(out);
    in.close();
    std::error_code ec;
    std::filesystem::remove(src, ec);
    if (ec) {
        std::cerr << "Failed to remove original file after gzip: "
                  << ec.message() << std::endl;
    }
}

// ============== 新增：工具函数 ==============

// 从 pattern 提取扩展名（如 ".txt"）；如果没有扩展名，返回空字符串
std::string RollingFileManager::expectedExtension() const {
    auto pos = pattern_.find_last_of('.');
    if (pos == std::string::npos) return {};
    return pattern_.substr(pos); // 包含点
}

// 查找最近的未压缩日志文件；若还未超 size/age，则返回其路径用于 append
std::filesystem::path RollingFileManager::findLatestAppendableFile() const {
    std::error_code ec;
    std::vector<std::filesystem::directory_entry> files;
    const auto wantExt = expectedExtension();

    for (auto& e : std::filesystem::directory_iterator(base_dir_, ec)) {
        if (ec) break;
        if (!e.is_regular_file()) continue;

        auto p = e.path();
        // 排除压缩文件
        if (p.extension() == ".gz") continue;

        // 若 pattern 指定了扩展名，则只匹配相同扩展
        if (!wantExt.empty() && p.extension() != wantExt) continue;

        files.push_back(e);
    }

    if (files.empty()) return {};

    // 最近的在前
    std::sort(files.begin(), files.end(), [](auto& a, auto& b) {
        std::error_code e1, e2;
        return a.last_write_time(e1) > b.last_write_time(e2);
    });

    auto candidate = files.front().path();

    try {
        // 大小检查
        auto sz = std::filesystem::file_size(candidate);
        if (sz >= max_bytes_) return {}; // 太大，不复用

        // 年龄检查：基于 mtime
        std::error_code ec2;
        auto mtime = std::filesystem::last_write_time(candidate, ec2);
        if (ec2) return {};
        auto now_file = std::filesystem::file_time_type::clock::now();
        if (now_file - mtime >= max_age_) return {}; // 太旧，不复用

        return candidate; // ✅ 可复用
    } catch (...) {
        return {};
    }
}
