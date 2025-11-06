#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

struct DiskPolicy {
    uint64_t soft_min_free_bytes;  // 软限制：清理到这个值
    uint64_t hard_min_free_bytes;  // 硬限制：低于此值暂停写入
    size_t min_keep_files;         // 最少保留文件数
};

class DiskSpaceGuard {
public:
    DiskSpaceGuard(const std::filesystem::path dir, 
                   std::string prefix, 
                   std::string ext, 
                   DiskPolicy policy);
    
    // 确保软限制（清理旧文件）
    bool ensureSoft();
    
    // 检查是否触发硬限制
    bool hardPressure() const;
    
    // 更新策略
    void setPolicy(const DiskPolicy& p);
    
    // 更新目录
    void setDir(const std::filesystem::path& dir);
    
private:
    std::filesystem::path dir_;
    std::string prefix_;
    std::string ext_;
    DiskPolicy policy_;
    
    static bool hasPrefix(const std::string name, const std::string& prefix);
    void collectCandidates(std::vector<std::filesystem::path>& gz, 
                          std::vector<std::filesystem::path>& txt) const;
    void reclaimUtilSoft();
};