#include "RollingFileManager.h"
#include <zlib.h>
#include <thread>
#include <sstream>
#include <iostream>
static std::string formatLine(int i){
    auto t=std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    std::tm tm=*std::localtime(&t);
    char ts[32];std::strftime(ts,sizeof(ts),"%Y-%m-%d %H:%M:%S",&tm);
    std::ostringstream oss;
    oss<<ts<<"INFO main.cpp:0 main - demo line"<<i;
    return oss.str();
}
static bool ensure_dir(const std::filesystem::path& p) {
    std::error_code ec;
    std::filesystem::create_directories(p, ec);
    if (ec) std::cerr << "Failed to create directory \"" << p << "\": " << ec.message() << "\n";
    return !ec;
}
int main(int argc, char** argv){
    // 优先级：命令行参数 > 环境变量 LOG_DIR > 默认 ./logs/day1
    std::filesystem::path logDir = (argc > 1) ? std::filesystem::path(argv[1])
                     : (std::getenv("LOG_DIR") ? std::filesystem::path(std::getenv("LOG_DIR"))
                                               : std::filesystem::path("./logs/day1"));

    if (!ensure_dir(logDir)) return 1;
    const std::string pattern="log_%Y%m%d_%H%M%S_%03d.log";
    const size_t maxBytes=1*1024*1024; // 1 MB
   const auto   maxAge    = std::chrono::minutes(1);
    const size_t reserveN=3;
    const bool compressOld=true;
    RollingFileManager cy(logDir,pattern,
                                maxBytes,maxAge,
                                reserveN,compressOld);
    std::cout<<"writing logs to "<<std::filesystem::absolute(logDir)<<std::endl;
    std::cout<<"Rotate by size: "<<maxBytes<<"bytes OR age >=1min"<<std::endl;
    for(int i=0;i<200000;++i){
        if(cy.needRotate()){
            cy.rotate();
        }
        auto& os=cy.stream();
        os<<formatLine(i)<<std::endl;
        if(i%1000==0){
            os.flush();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    std::cout<<"done.check log files under"<<logDir<<std::endl;
    return 0;}