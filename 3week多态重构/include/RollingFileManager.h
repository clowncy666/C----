#pragma once
#include <filesystem>
#include <fstream>
#include <chrono>
#include <string>
#include <vector>
#include "DiskSpaceGuard.h"
#include <unistd.h>
#include <limits.h>

// 获取进程名称
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
/*
// 获取进程 PID
inline int getPid() {
    return (int)::getpid();
}
*/
// 构建进程专属目录：<base_dir>/<proc_name>/<pid>/
inline std::filesystem::path getProcessLogDir(const std::filesystem::path& base_dir) {
    std::string pname = getProcessName();
    //int pid = getPid();
    std::filesystem::path path = base_dir / pname; // std::to_string(pid);
    std::filesystem::create_directories(path);
    return path;
}

class RollingFileManager {
public:
    RollingFileManager(std::filesystem::path baseDir,
                      std::string pattern,
                      size_t maxBytes,
                      std::chrono::minutes maxAge,
                      size_t reserveN,
                      bool compressOld);
    
    ~RollingFileManager();
    
    std::ofstream& stream();
    std::filesystem::path currentPath() const;
    
    bool needRotate();
    void rotate();
    bool ensureWritable(size_t bytes_hint);
    
private:
    void rollToNewFile();
    void enforceReserveN();
    void gzipFile(const std::filesystem::path& src);
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
    
    std::filesystem::path current_path_;
    std::ofstream ofs_;
    
    DiskSpaceGuard guard_;
    bool suspend_writes_ = false;
};