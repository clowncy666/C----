#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>
#include <functional>
struct DiskPolicy {
    uint64_t soft_min_free_bytes;  // 软限制：清理到这个值
    uint64_t hard_min_free_bytes;  // 硬限制：低于此值暂停写入
    size_t min_keep_files;         // 最少保留文件数
    // 改进：提供便捷构造函数
    static DiskPolicy fromMB(uint64_t soft_mb, uint64_t hard_mb, size_t min_files) {
        constexpr uint64_t MB = 1024 * 1024;
        return {soft_mb * MB, hard_mb * MB, min_files};
    }
    
    // 验证配置合法性
    bool isValid() const {
        return soft_min_free_bytes > hard_min_free_bytes && 
               hard_min_free_bytes > 0 && 
               min_keep_files > 0;
    }
};

// 文件回收策略接口（多态扩展点）
class IReclaimStrategy {
public:
    virtual ~IReclaimStrategy() = default;
    
    // 返回应该删除的文件列表（已排序）
    virtual std::vector<std::filesystem::path> selectFilesToRemove(
        const std::vector<std::filesystem::path>& candidates,
        size_t max_to_remove) const = 0;
};

// 默认策略：优先删除 .gz，然后按时间排序
class DefaultReclaimStrategy : public IReclaimStrategy {
public:
    std::vector<std::filesystem::path> selectFilesToRemove(
        const std::vector<std::filesystem::path>& candidates,
        size_t max_to_remove) const override;
};




class DiskSpaceGuard {
public:
    DiskSpaceGuard(const std::filesystem::path dir, 
                   std::string prefix, 
                   std::string ext, 
                   DiskPolicy policy);
    // 改进：支持策略注入
    DiskSpaceGuard(std::filesystem::path dir, 
                   std::string prefix, 
                   std::string ext, 
                   DiskPolicy policy,
                   std::shared_ptr<IReclaimStrategy> strategy);
    // 确保软限制（清理旧文件）
    bool ensureSoft();
    
    // 检查是否触发硬限制
    bool hardPressure() const;
    
    // 更新策略
    void setPolicy(const DiskPolicy& p);
    
    // 更新目录
    void setDir(const std::filesystem::path& dir);
    void setReclaimStrategy(std::shared_ptr<IReclaimStrategy> strategy);
    using OnReclaimCallback = std::function<void(const std::filesystem::path&)>;
    void setOnReclaimCallback(OnReclaimCallback callback);


private:
    std::filesystem::path dir_;
    std::string prefix_;
    std::string ext_;
    DiskPolicy policy_;
    
    static bool hasPrefix(const std::string name, const std::string& prefix);
    void collectCandidates(std::vector<std::filesystem::path>& gz, 
                          std::vector<std::filesystem::path>& txt) const;
    void reclaimUtilSoft();
    bool tryRemoveFile(const std::filesystem::path& path);
    std::shared_ptr<IReclaimStrategy> reclaim_strategy_;
    OnReclaimCallback on_reclaim_;
};