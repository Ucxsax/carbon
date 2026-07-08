#include "carbon/utils/logger.h"
#include <iostream>

namespace carbon {

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

void Logger::SetLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
}

void Logger::SetCallback(LogCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = std::move(callback);
}

void Logger::Log(LogLevel level, const std::string& message) {
    if (level < level_) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (callback_) {
        callback_(level, message);
    } else {
        const char* level_str = "UNKNOWN";
        switch (level) {
            case LogLevel::Debug:   level_str = "DEBUG"; break;
            case LogLevel::Info:    level_str = "INFO"; break;
            case LogLevel::Warning: level_str = "WARN"; break;
            case LogLevel::Error:   level_str = "ERROR"; break;
            case LogLevel::Fatal:   level_str = "FATAL"; break;
        }
        std::cerr << "[Carbon][" << level_str << "] " << message << std::endl;
    }
}

} // namespace carbon
