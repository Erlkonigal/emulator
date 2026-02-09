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
    LogLevel Level = LogLevel::Info;
    std::string DeviceOutput;  // Device output file (empty = stdout)
    std::string LogOutput;     // Log/error output file (empty = stderr)
    
    bool IsDualOutput() const {
        return !DeviceOutput.empty() || !LogOutput.empty();
    }
};

void LogInit(const LogConfig& config);
void LogSetLevel(LogLevel level);
void LogSetOutputHandler(std::function<void(const char*)> handler);
void LogMessage(LogLevel level, const char* file, int line, const char* fmt, ...);
void LogPrint(const char* fmt, ...);
void LogDevicePrint(const char* fmt, ...);

#define LOG_TRACE(...) LogMessage(LogLevel::Trace, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(...) LogMessage(LogLevel::Debug, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  LogMessage(LogLevel::Info,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  LogMessage(LogLevel::Warn,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) LogMessage(LogLevel::Error, __FILE__, __LINE__, __VA_ARGS__)

#endif
