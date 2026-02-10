#ifndef EMULATOR_LOGGING_LOGGING_H
#define EMULATOR_LOGGING_LOGGING_H

#include <string>
#include <cstdarg>
#include <functional>

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error
};

struct LogConfig {
    LogLevel level = LogLevel::Info;
    std::string deviceOutput;
    std::string logOutput;
    
    bool isDualOutput() const {
        return !deviceOutput.empty() || !logOutput.empty();
    }
};

void logInit(const LogConfig& config);
void logSetLevel(LogLevel level);
void logSetOutputHandler(std::function<void(const char*)> handler);
void logMessage(LogLevel level, const char* file, int line, const char* fmt, ...);
void logPrint(const char* fmt, ...);
void logDevicePrint(const char* fmt, ...);

#define LOG_TRACE(...) logMessage(LogLevel::Trace, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(...) logMessage(LogLevel::Debug, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  logMessage(LogLevel::Info,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  logMessage(LogLevel::Warn,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) logMessage(LogLevel::Error, __FILE__, __LINE__, __VA_ARGS__)

#endif
