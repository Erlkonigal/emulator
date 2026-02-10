#ifndef EMULATOR_LOGGING_LOGGER_H
#define EMULATOR_LOGGING_LOGGER_H

#include <cstdarg>
#include <functional>
#include <string>

namespace logging {

enum class Level {
    Trace,
    Debug,
    Info,
    Warn,
    Error
};

struct Config {
    Level level = Level::Info;
    std::string mFile;
    std::string mDeviceFile;
    std::function<void(const char*)> mOnMessage;
    std::function<void(const char*)> mOnDeviceMessage;
};

void init(const Config& config);
void level(Level newLevel);
void setOutputHandler(std::function<void(const char*)> logHandler,
                        std::function<void(const char*)> deviceHandler);
void info(const char* file, int line, const char* fmt, ...);
void debug(const char* file, int line, const char* fmt, ...);
void warn(const char* file, int line, const char* fmt, ...);
void error(const char* file, int line, const char* fmt, ...);
void trace(const char* file, int line, const char* fmt, ...);
void raw(const char* fmt, ...);
void device(const char* fmt, ...);

const char* levelToString(Level level);
Level levelFromString(const std::string& str);

} // namespace logging

#define INFO(...)    logging::info(__FILE__, __LINE__, __VA_ARGS__)
#define DEBUG(...)   logging::debug(__FILE__, __LINE__, __VA_ARGS__)
#define WARN(...)    logging::warn(__FILE__, __LINE__, __VA_ARGS__)
#define ERROR(...)   logging::error(__FILE__, __LINE__, __VA_ARGS__)
#define TRACE(...)  logging::trace(__FILE__, __LINE__, __VA_ARGS__)

#endif // EMULATOR_LOGGING_LOGGER_H
