#pragma once
#include <filesystem>
#include <fstream>
#include <chrono>
#include <string>
#include <vector>
#include "DiskSpaceGuard.h"
#include <unistd.h>
#include <limits.h>

namespace ProcessUtils {
    inline std::string getProcessName() {
        char buf[PATH_MAX]{};
        ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf)-1);
        if (n > 0) {
            std::string path(buf, n);
            auto pos = path.find_last_of('/');
            return (pos == std::string::npos) ? path : path.substr(pos + 1);
        }
        return "unknown";
    }

    inline std::filesystem::path getProcessLogDir(const std::filesystem::path& base_dir) {
        std::string pname = getProcessName();
        std::filesystem::path path = base_dir / pname;
        std::filesystem::create_directories(path);
        return path;
    }
}
class IRotationPolicy {
public:
    virtual ~IRotationPolicy() = default;
    virtual bool shouldRotate(size_t current_size, 
                             std::chrono::minutes age,
                             size_t max_bytes,
                             std::chrono::minutes max_age) const = 0;
};

// 默认混合策略：大小或时间任一触发
class HybridRotationPolicy : public IRotationPolicy {
public:
    bool shouldRotate(size_t current_size, 
                     std::chrono::minutes age,
                     size_t max_bytes,
                     std::chrono::minutes max_age) const override {
        return current_size >= max_bytes || age >= max_age;
    }
};

// 文件压缩策略接口（多态扩展点）
class ICompressionStrategy {
public:
    virtual ~ICompressionStrategy() = default;
    virtual bool compress(const std::filesystem::path& src) = 0;
    virtual std::string compressedExtension() const = 0;
};



class RollingFileManager {
public:
    
    // 改进：使用配置结构
    struct Config {
        std::filesystem::path base_dir;
        std::string pattern;
        size_t max_bytes;
        std::chrono::minutes max_age;
        size_t reserve_n;
        bool compress_old;
        
        // 新增：策略注入
        std::shared_ptr<IRotationPolicy> rotation_policy;
        std::shared_ptr<ICompressionStrategy> compression_strategy;
    };
    
    // 构造函数：支持策略注入
    explicit RollingFileManager(Config config);
    
    // 传统构造（向后兼容）
    RollingFileManager(std::filesystem::path baseDir,
                      std::string pattern,
                      size_t maxBytes,
                      std::chrono::minutes maxAge,
                      size_t reserveN,
                      bool compressOld);
    // 禁止拷贝
    RollingFileManager(const RollingFileManager&) = delete;
    RollingFileManager& operator=(const RollingFileManager&) = delete;
    
    // 允许移动
    RollingFileManager(RollingFileManager&&) noexcept = default;
    RollingFileManager& operator=(RollingFileManager&&) noexcept = default;
    
    ~RollingFileManager();

    std::ofstream& stream();
    std::filesystem::path currentPath() const;
    
    bool needRotate();
    void rotate();
    bool ensureWritable(size_t bytes_hint);
    
private:
    void rollToNewFile();
    void enforceReserveN();
    void compressFile(const std::filesystem::path& src);
    std::string nowStr(const char* fmt) const;
    std::string makeFilename(int seq) const;
    std::string expectedExtension() const;
    std::filesystem::path findLatestAppendableFile() const;
    
    std::filesystem::path base_dir_;
    std::string pattern_;
    size_t max_bytes_;
    std::chrono::minutes max_age_;
    size_t reserve_n_;
    bool compress_;
    
    // 策略对象
    std::shared_ptr<IRotationPolicy> rotation_policy_;
    std::shared_ptr<ICompressionStrategy> compression_strategy_;
    
    // 运行时状态
    std::filesystem::path current_path_;
    std::ofstream ofs_;
    std::chrono::system_clock::time_point file_created_time_;
    
    std::unique_ptr<DiskSpaceGuard> guard_;
    bool suspend_writes_ = false;
};