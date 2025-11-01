#include "RollingFileManager.h"
#include <zlib.h>
#include <sstream>
#include <iostream>
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
    std::filesystem::create_directories(base_dir_,ec);
    if(ec){
        std::cerr<<"Failed to create base directory: "<<ec.message()<<std::endl;
    }
    rollToNewFile();
}
RollingFileManager::~RollingFileManager(){
    if(ofs_.is_open()){
        ofs_.close();
    }
}
std::ofstream& RollingFileManager::stream(){
    return ofs_;
}
std::filesystem::path RollingFileManager::currentPath() const{
    return current_path_;
}
bool RollingFileManager::needRotate(){
    if(!ofs_.is_open()){
        return true;
    }
    try{
        auto sz=std::filesystem::file_size(current_path_);
        if(sz>=max_bytes_){
            return true;    
        }

    }catch(...){
        return true;
    }
    auto now=std::chrono::system_clock::now();
    if(now - open_time_ >= max_age_){
        return true;
    }
    return false;
}
void RollingFileManager::rotate(){
    if(ofs_.is_open()){
        ofs_.flush();
        ofs_.close();
    }
    if(compress_){
        try{gzipFile(current_path_);}
        catch(...){}
    }
    enforceReserveN();
    rollToNewFile();
    
}
void RollingFileManager::enforceReserveN(){
    std::vector<std::filesystem::directory_entry> entries;
    for(auto& entry: std::filesystem::directory_iterator(base_dir_)){
        if(entry.is_regular_file()){
            entries.push_back(entry);
        }
    }
    std::sort(entries.begin(),entries.end(),
              [](const std::filesystem::directory_entry& a,
                 const std::filesystem::directory_entry& b){
                    return a.last_write_time() > b.last_write_time();
              });
    for(size_t i=reserve_n_; i<entries.size(); ++i){
        std::error_code ec;
        std::filesystem::remove(entries[i].path(),ec);
        if(ec){
            std::cerr<<"Failed to remove old log file "<<entries[i].path()
                     <<": "<<ec.message()<<std::endl;
        }
    }
}

std::string RollingFileManager::nowStr(const char* fmt){
    auto t=std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    std::tm tm=*std::localtime(&t);
    char buf[64]{};
    std::strftime(buf,sizeof(buf),fmt,&tm);
    return buf;
}

std::string RollingFileManager::makeFilename(int seq) const{
    std::string ts=nowStr("%Y%m%d_%H%M%S");
    auto p=pattern_;
    auto pos=p.find("%Y%m%d_%H%M%S");
    if (pos!=std::string::npos){
        p.replace(pos,15,ts);
    }
    pos=p.find("%03d");
    if(pos!=std::string::npos){
        std::ostringstream oss;
        oss<<std::setw(3)<<std::setfill('0')<<seq;
        p.replace(pos,4,oss.str());
    }
    return p;
}
void RollingFileManager::rollToNewFile(){
    for (int i=0;i<1000;++i){
        auto candidate=base_dir_/makeFilename(i);
        std::error_code ec;
        if(!std::filesystem::exists(candidate,ec)){
            current_path_=candidate;
            ofs_.open(current_path_,std::ios::out|std::ios::app);
            open_time_=std::chrono::system_clock::now();
            return;
        }
    }
    current_path_=base_dir_/makeFilename(999);
    ofs_.open(current_path_,std::ios::out|std::ios::app);
    open_time_=std::chrono::system_clock::now();
}
void RollingFileManager::gzipFile(const std::filesystem::path& src){
    std::ifstream in(src,std::ios::binary);
    if(!in){return;}
    auto gzPath=src.string()+".gz";
    gzFile out=gzopen(gzPath.c_str(),"wb");
    if(!out){return; }
    char buffer[1<<16];
    while(in){
        in.read(buffer,sizeof(buffer));
        auto n=in.gcount();
        if(n>0){gzwrite(out,buffer,static_cast<unsigned int>(n));}
    }
    gzclose(out);
    in.close();
    std::error_code ec;
    std::filesystem::remove(src,ec);
    if(ec){
        std::cerr<<"Failed to remove original file after gzip: "<<ec.message()<<std::endl;
    }
}
