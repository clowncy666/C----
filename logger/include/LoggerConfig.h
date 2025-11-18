#include <fstream>
#include <stdexcept>
using json = nlohmann::json; 
enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};
//单个模块配置
struct ModuleConfig {
    std::string name;
    std::string pattern;
    size_t max_bytes = 1 * 1024 * 1024;  // 1MB
    std::chrono::minutes max_age{60};
    size_t reserve_n = 8;
    bool compress_old = true;
    
    // 从JSON加载
    static ModuleConfig fromJson(const json& j) {
        ModuleConfig cfg;
        cfg.name = j.value("name", "default");
        cfg.pattern = j.value("pattern", "log_%Y%m%d_%H%M%S_%03d.txt");
        cfg.max_bytes = j.value("max_bytes_mb", 1) * 1024 * 1024;
        cfg.max_age = std::chrono::minutes(j.value("max_age_minutes", 60));
        cfg.reserve_n = j.value("reserve_n", 8);
        cfg.compress_old = j.value("compress_old", true);
        return cfg;
    }
    
    // 转为JSON
    json toJson() const {
        return {
            {"name", name},
            {"pattern", pattern},
            {"max_bytes_mb", max_bytes / (1024 * 1024)},
            {"max_age_minutes", max_age.count()},
            {"reserve_n", reserve_n},
            {"compress_old", compress_old}
        };
    }
};

// 全局日志配置
struct LoggerConfig {
    // 基础路径
    std::filesystem::path base_dir = "./logs";
    
    // 全局设置
    LogLevel log_level = LogLevel::INFO;
    bool async_mode = true;
    size_t async_queue_size = 10000;
    
    // 模块配置（支持多模块）
    std::vector<ModuleConfig> modules;
    

    // 从JSON文件加载
    static LoggerConfig fromFile(const std::string& config_path) {
        std::ifstream ifs(config_path);
        if (!ifs.is_open()) {
            throw std::runtime_error("Failed to open config file: " + config_path);
        }
        
        json j;
        ifs >> j;
        return fromJson(j);
    }
    
    // 从JSON对象加载
    static LoggerConfig fromJson(const json& j) {
        LoggerConfig cfg;
        
        // 基础路径
        cfg.base_dir = j.value("base_dir", "./logs");
        
        // 全局设置
        std::string level_str = j.value("log_level", "INFO");
        cfg.log_level = parseLogLevel(level_str);
        cfg.async_mode = j.value("async_mode", true);
        cfg.async_queue_size = j.value("async_queue_size", 10000);
        
        // 加载模块配置
        if (j.contains("modules") && j["modules"].is_array()) {
            for (const auto& mod : j["modules"]) {
                cfg.modules.push_back(ModuleConfig::fromJson(mod));
            }
        } else {
            // 默认三个模块
            cfg.modules = createDefaultModules();
        }
        
        return cfg;
    }
    
    // 保存到JSON文件
    void saveToFile(const std::string& config_path) const {
        json j = toJson();
        std::ofstream ofs(config_path);
        if (!ofs.is_open()) {
            throw std::runtime_error("Failed to write config file: " + config_path);
        }
        ofs << j.dump(4);  // 美化输出
    }
    
    // 转为JSON
    json toJson() const {
        json j;
        j["base_dir"] = base_dir.string();
        j["log_level"] = logLevelToString(log_level);
        j["async_mode"] = async_mode;
        j["async_queue_size"] = async_queue_size;
        
        json modules_json = json::array();
        for (const auto& mod : modules) {
            modules_json.push_back(mod.toJson());
        }
        j["modules"] = modules_json;
        
        return j;
    }
    // 查找模块配置
    const ModuleConfig* findModule(const std::string& name) const {
        for (const auto& mod : modules) {
            if (mod.name == name) return &mod;
        }
        return nullptr;
    }
    
private:
    static LogLevel parseLogLevel(const std::string& level_str) {
        if (level_str == "DEBUG") return LogLevel::DEBUG;
        if (level_str == "INFO") return LogLevel::INFO;
        if (level_str == "WARNING") return LogLevel::WARNING;
        if (level_str == "ERROR") return LogLevel::ERROR;
        if (level_str == "CRITICAL") return LogLevel::CRITICAL;
        return LogLevel::INFO;
    }
    
    static std::string logLevelToString(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARNING: return "WARNING";
            case LogLevel::ERROR: return "ERROR";
            case LogLevel::CRITICAL: return "CRITICAL";
            default: return "INFO";
        }
    }
    
    static std::vector<ModuleConfig> createDefaultModules() {
        return {
            ModuleConfig{
                "text", "log_%Y%m%d_%H%M%S_%03d.txt",
                1 * 1024 * 1024, std::chrono::minutes(60), 8, true
            },
            ModuleConfig{
                "binary", "binary_%Y%m%d_%H%M%S_%03d.bin",
                5 * 1024 * 1024, std::chrono::minutes(120), 5, true
            },
            ModuleConfig{
                "bag", "messages_%Y%m%d_%H%M%S_%03d.bag",
                10 * 1024 * 1024, std::chrono::minutes(180), 3, true
            }
        };
    }
};