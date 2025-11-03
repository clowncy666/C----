#include "DiskSpaceGuard.h"
#include <filesystem>
#include <algorithm>
#include <iostream>

namespace fs=std::filesystem;
static uint64_t freeBytes(const fs::path& p)
{
    std::error_code ec;
    auto space=fs::space(p,ec);
    if(ec)return 0;
    return (uint64_t)space.available;
}
DiskSpaceGuard::DiskSpaceGuard(const fs::path dir,std::string prefix,std::string ext,DiskPolicy policy)
    :dir_(dir),prefix_(prefix),ext_(ext),policy_(policy){}
bool DiskSpaceGuard::ensureSoft(){
    if(freeBytes(dir_)>=policy_.soft_min_free_bytes){
        return true;
    }
    reclaimUtilSoft();
    return freeBytes(dir_)>=policy_.soft_min_free_bytes;
}
bool DiskSpaceGuard::hardPressure() const{return freeBytes(dir_)<policy_.hard_min_free_bytes;}
void DiskSpaceGuard::setPolicy(const DiskPolicy& p){policy_=p;}
void DiskSpaceGuard::setDir(const fs::path& dir){dir_=dir;}
bool DiskSpaceGuard::hasPrefix(const std::string name,const std::string& prefix){return name.rfind(prefix,0)==0;}
void DiskSpaceGuard::collectCandidates(std::vector<fs::path>& gz,std::vector<fs::path>& txt)const{
    gz.clear();
    txt.clear();
    std::error_code ec;
    for(const auto& e:fs::directory_iterator(dir_,ec)){
        if(ec)break;
        if(!e.is_regular_file())continue;
        const auto& p=e.path();
        const auto fname=p.filename().string();
        if(!prefix_.empty()&&!hasPrefix(fname,prefix_))continue;
        if(p.extension()==".gz"){
            if(p.stem().extension().string()==ext_){
                gz.push_back(p);
            }
        }
        else if(p.extension()==ext_){
            txt.push_back(p);
        }
    }
    auto by_time_asc=[](const fs::path& a,const fs::path& b){
        std::error_code e1,e2;
        return fs::last_write_time(a,e1)<fs::last_write_time(b,e2); 
    };
    std::sort(gz.begin(),gz.end(),by_time_asc);
    std::sort(txt.begin(),txt.end(),by_time_asc);
}  
void DiskSpaceGuard::reclaimUtilSoft(){
    std::vector<fs::path> gz, txt;
    collectCandidates(gz, txt);
    auto count_total=gz.size()+txt.size();
    auto must_keep=policy_.min_keep_files;
    if(count_total<=must_keep)return;
    auto try_move=[&](const fs::path& p){
        std::error_code ec;
        fs::remove(p,ec);
        if(ec){
            std::cerr<<"Failed to remove file "<<p<<": "<<ec.message()<<"\n";
        }
    };
    for(size_t i=0;i<gz.size()&&(freeBytes(dir_)<policy_.soft_min_free_bytes);++i){
        if(--(count_total)<=must_keep)break;
        try_move(gz[i]);
    }
    for(size_t i=0;i<txt.size()&&(freeBytes(dir_)<policy_.soft_min_free_bytes);++i){
        if(--(count_total)<=must_keep)break;
        try_move(txt[i]);
    }

}     

