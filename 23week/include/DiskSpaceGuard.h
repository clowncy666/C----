#pragma once
#include <filesystem>
#include <cstdint>
#include <vector>
struct DiskPolicy
{
    uint64_t soft_min_free_bytes=512*1024*1024; // 512MB
    uint64_t hard_min_free_bytes=128*1024*1024; // 128MB
    size_t min_keep_files=3;
};
class DiskSpaceGuard
{
public:
    DiskSpaceGuard(const std::filesystem::path dir,std::string prefix,std::string ext,DiskPolicy policy);
    bool ensureSoft();
    bool ensurePresure()const;
    void setPolicy(const DiskPolicy& p);
    bool hardPressure() const;
    
    // 目录可能变（比如后面做按进程分目录），支持更新
    void setDir(const std::filesystem::path& dir);
private:
    std::filesystem::path dir_;
    std::string prefix_;
    std::string ext_;
    DiskPolicy policy_;
    void reclaimUtilSoft();
    void collectCandidates(std::vector<std::filesystem::path>& gz,std::vector<std::filesystem::path>& txt)const;
    static bool hasPrefix(const std::string name,const std::string& prefix);
};