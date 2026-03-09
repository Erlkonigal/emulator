#pragma once

#include <cstdio>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>

#include "emulator/utils/singleton.h"

class Logger : public Singleton<Logger> {
public:
    enum class Level { Debug, Info, Warn, Error };

    struct Config {
        Level level = Level::Info;
        std::string filePath;
        std::function<void(const char*)> handler;
    };

    void init(const Config& config);
    void setLevel(Level level);
    void setHandler(std::function<void(const char*)> handler);

    void log(Level level, const char* file, int line, std::string_view fmt, ...);
    void raw(std::string_view fmt, ...);

    static constexpr const char* levelToString(Level level);
    static Level levelFromString(std::string_view str);

private:
    std::mutex mMutex;
    Level mLevel = Level::Info;
    std::function<void(const char*)> mHandler;
    std::string mFilePath;
    FILE* mFile = nullptr;

    Logger() = default;
    ~Logger();
    friend class Singleton<Logger>;
};

#define INFO(...)  Logger::getInstance().log(Logger::Level::Info, __FILE__, __LINE__, __VA_ARGS__)
#define DEBUG(...) Logger::getInstance().log(Logger::Level::Debug, __FILE__, __LINE__, __VA_ARGS__)
#define WARN(...)  Logger::getInstance().log(Logger::Level::Warn, __FILE__, __LINE__, __VA_ARGS__)
#define ERROR(...) Logger::getInstance().log(Logger::Level::Error, __FILE__, __LINE__, __VA_ARGS__)
#define RAW(...)   Logger::getInstance().raw(__VA_ARGS__)
#define LINE(...) do { RAW(__VA_ARGS__); RAW("\n"); } while(0)