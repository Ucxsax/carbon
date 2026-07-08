#pragma once

#include <string>
#include <functional>
#include <mutex>

namespace carbon {

enum class LogLevel : uint8_t {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
    Fatal = 4
};

using LogCallback = std::function<void(LogLevel level, const std::string& message)>;

class Logger {
public:
    static Logger& Instance();
    
    void SetLevel(LogLevel level);
    LogLevel GetLevel() const { return level_; }
    
    void SetCallback(LogCallback callback);
    
    void Log(LogLevel level, const std::string& message);
    void Debug(const std::string& message) { Log(LogLevel::Debug, message); }
    void Info(const std::string& message) { Log(LogLevel::Info, message); }
    void Warning(const std::string& message) { Log(LogLevel::Warning, message); }
    void Error(const std::string& message) { Log(LogLevel::Error, message); }
    
private:
    Logger() = default;
    
    LogLevel level_ = LogLevel::Info;
    LogCallback callback_;
    std::mutex mutex_;
};

// 便捷宏
#define CARBON_LOG_DEBUG(msg)   carbon::Logger::Instance().Debug(msg)
#define CARBON_LOG_INFO(msg)    carbon::Logger::Instance().Info(msg)
#define CARBON_LOG_WARN(msg)    carbon::Logger::Instance().Warning(msg)
#define CARBON_LOG_ERROR(msg)   carbon::Logger::Instance().Error(msg)

} // namespace carbon
