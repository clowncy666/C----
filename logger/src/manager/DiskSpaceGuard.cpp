#include "DiskSpaceGuard.h"
#include <filesystem>
#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;

static uint64_t freeBytes(const fs::path& p) {
    std::error_code ec;
    auto space = fs::space(p, ec);
    if (ec) return 0;
    return (uint64_t)space.available;
}

// ============================================
// 默认回收策略实现
// ============================================
std::vector<std::filesystem::path> DefaultReclaimStrategy::selectFilesToRemove(
    const std::vector<std::filesystem::path>& candidates,
    size_t max_to_remove) const {
    
    if (candidates.empty() || max_to_remove == 0) {
        return {};
    }
    
    // 复制候选列表并按时间排序（最旧的优先）
    auto sorted = candidates;
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
        std::error_code e1, e2;
        return fs::last_write_time(a, e1) < fs::last_write_time(b, e2);
    });
    
    // 返回前 max_to_remove 个文件
    size_t count = std::min(max_to_remove, sorted.size());
    return std::vector<fs::path>(sorted.begin(), sorted.begin() + count);
}

// ============================================
// DiskSpaceGuard 实现
// ============================================
DiskSpaceGuard::DiskSpaceGuard(const fs::path dir, 
                               std::string prefix, 
                               std::string ext, 
                               DiskPolicy policy)
    : dir_(dir), prefix_(std::move(prefix)), ext_(std::move(ext)), 
      policy_(policy), reclaim_strategy_(std::make_shared<DefaultReclaimStrategy>()) {}

DiskSpaceGuard::DiskSpaceGuard(fs::path dir, 
                               std::string prefix, 
                               std::string ext, 
                               DiskPolicy policy,
                               std::shared_ptr<IReclaimStrategy> strategy)
    : dir_(std::move(dir)), prefix_(std::move(prefix)), ext_(std::move(ext)), 
      policy_(policy), reclaim_strategy_(std::move(strategy)) {
    if (!reclaim_strategy_) {
        reclaim_strategy_ = std::make_shared<DefaultReclaimStrategy>();
    }
}

bool DiskSpaceGuard::ensureSoft() {
    if (freeBytes(dir_) >= policy_.soft_min_free_bytes) {
        return true;
    }
    reclaimUtilSoft();
    return freeBytes(dir_) >= policy_.soft_min_free_bytes;
}

bool DiskSpaceGuard::hardPressure() const {
    return freeBytes(dir_) < policy_.hard_min_free_bytes;
}

void DiskSpaceGuard::setPolicy(const DiskPolicy& p) {
    policy_ = p;
}

void DiskSpaceGuard::setDir(const fs::path& dir) {
    dir_ = dir;
}

void DiskSpaceGuard::setReclaimStrategy(std::shared_ptr<IReclaimStrategy> strategy) {
    if (strategy) {
        reclaim_strategy_ = std::move(strategy);
    }
}

void DiskSpaceGuard::setOnReclaimCallback(OnReclaimCallback callback) {
    on_reclaim_ = std::move(callback);
}

uint64_t DiskSpaceGuard::getAvailableBytes() const {
    return freeBytes(dir_);
}

size_t DiskSpaceGuard::countManagedFiles() const {
    std::vector<fs::path> gz, txt;
    collectCandidates(gz, txt);
    return gz.size() + txt.size();
}

bool DiskSpaceGuard::hasPrefix(const std::string& name, const std::string& prefix) {
    return name.rfind(prefix, 0) == 0;
}

void DiskSpaceGuard::collectCandidates(std::vector<fs::path>& gz, 
                                       std::vector<fs::path>& txt) const {
    gz.clear();
    txt.clear();
    std::error_code ec;
    
    for (const auto& e : fs::directory_iterator(dir_, ec)) {
        if (ec) break;
        if (!e.is_regular_file()) continue;
        
        const auto& p = e.path();
        const auto fname = p.filename().string();
        
        if (!prefix_.empty() && !hasPrefix(fname, prefix_)) continue;
        
        if (p.extension() == ".gz") {
            if (p.stem().extension().string() == ext_) {
                gz.push_back(p);
            }
        } else if (p.extension() == ext_) {
            txt.push_back(p);
        }
    }
    
    auto by_time_asc = [](const fs::path& a, const fs::path& b) {
        std::error_code e1, e2;
        return fs::last_write_time(a, e1) < fs::last_write_time(b, e2);
    };
    
    std::sort(gz.begin(), gz.end(), by_time_asc);
    std::sort(txt.begin(), txt.end(), by_time_asc);
}

bool DiskSpaceGuard::tryRemoveFile(const fs::path& path) {
    std::error_code ec;
    fs::remove(path, ec);
    
    if (!ec) {
        if (on_reclaim_) {
            on_reclaim_(path);
        }
        return true;
    } else {
        std::cerr << "Failed to remove file " << path << ": " 
                  << ec.message() << "\n";
        return false;
    }
}

void DiskSpaceGuard::reclaimUtilSoft() {
    std::vector<fs::path> gz, txt;
    collectCandidates(gz, txt);
    
    auto count_total = gz.size() + txt.size();
    auto must_keep = policy_.min_keep_files;
    if (count_total <= must_keep) return;
    
    // 使用策略选择要删除的文件
    size_t can_remove = count_total - must_keep;
    
    // 优先删除压缩文件
    if (!gz.empty()) {
        auto to_remove = reclaim_strategy_->selectFilesToRemove(gz, 
            std::min(can_remove, gz.size()));
        
        for (const auto& p : to_remove) {
            if (freeBytes(dir_) >= policy_.soft_min_free_bytes) break;
            if (tryRemoveFile(p)) {
                --count_total;
            }
        }
    }
    
    // 如果还不够，删除未压缩文件
    if (freeBytes(dir_) < policy_.soft_min_free_bytes && !txt.empty()) {
        can_remove = count_total - must_keep;
        auto to_remove = reclaim_strategy_->selectFilesToRemove(txt, 
            std::min(can_remove, txt.size()));
        
        for (const auto& p : to_remove) {
            if (freeBytes(dir_) >= policy_.soft_min_free_bytes) break;
            if (count_total <= must_keep) break;
            if (tryRemoveFile(p)) {
                --count_total;
            }
        }
    }
}