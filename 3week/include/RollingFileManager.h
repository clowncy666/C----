#pragma once
#include <filesystem>
#include <fstream>
#include <chrono>
#include <string>
#include <vector>
#include <algorithm>
#include <iomanip>

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

    // 是否需要轮转（依据大小与时间）
    bool needRotate();

    void rotate();

    void enforceReserveN();

private:
    std::filesystem::path base_dir_;
    std::filesystem::path current_path_;
    std::string pattern_;
    size_t max_bytes_;
    std::chrono::minutes max_age_;
    size_t reserve_n_;
    bool compress_;
    std::ofstream ofs_;
    std::chrono::system_clock::time_point open_time_{};

    static std::string nowStr(const char* fmt = "%Y%m%d_%H%M%S");
    std::string makeFilename(int seq) const;
    void rollToNewFile();
    static void gzipFile(const std::filesystem::path& src);
};
