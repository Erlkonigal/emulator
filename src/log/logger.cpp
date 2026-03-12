#include "emulator/log/logger.h"

#include <cstdarg>
#include <cstring>
#include <ctime>

namespace emulator {

namespace {

constexpr size_t kBufferSize = 4096;

const char* extractFilename(const char* path) {
    const char* result = path;
    for (const char* p = path; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            result = p + 1;
        }
    }
    return result;
}

} // namespace

Logger::~Logger() {
    if (mFile != nullptr && mFile != stderr) {
        std::fclose(mFile);
    }
}

void Logger::init(const Config& config) {
    std::lock_guard<std::mutex> lock(mMutex);
    mLevel = config.level;
    mHandler = config.handler;
    
    if (mFile != nullptr && mFile != stderr) {
        std::fclose(mFile);
        mFile = nullptr;
    }
    
    mFilePath = config.filePath;
    if (!mFilePath.empty()) {
        mFile = std::fopen(mFilePath.c_str(), "a");
        if (mFile == nullptr) {
            mFile = stderr;
        }
    }
}

void Logger::setLevel(Level level) {
    std::lock_guard<std::mutex> lock(mMutex);
    mLevel = level;
}

void Logger::setHandler(std::function<void(const char*)> handler) {
    std::lock_guard<std::mutex> lock(mMutex);
    mHandler = std::move(handler);
}

void Logger::log(Level level, const char* file, int line, std::string_view fmt, ...) {
    if (level < mLevel) return;

    std::lock_guard<std::mutex> lock(mMutex);

    if (level < mLevel) return;

    time_t now = std::time(nullptr);
    struct tm tmBuf;
    localtime_r(&now, &tmBuf);

    char timeBuf[64];
    std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tmBuf);

    char buffer[kBufferSize];
    int pos = std::snprintf(buffer, sizeof(buffer), "[%s] [%s] %s:%d: ",
                            timeBuf, levelToString(level), extractFilename(file), line);

    if (pos > 0 && static_cast<size_t>(pos) < sizeof(buffer)) {
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(buffer + pos, sizeof(buffer) - pos, fmt.data(), args);
        va_end(args);

        size_t len = std::strlen(buffer);
        if (len < sizeof(buffer) - 1) {
            buffer[len] = '\n';
            buffer[len + 1] = '\0';
        }
    }

    buffer[sizeof(buffer) - 1] = '\0';

    if (mHandler) {
        mHandler(buffer);
    }
    if (mFile != nullptr) {
        std::fputs(buffer, mFile);
        std::fflush(mFile);
    }
}

void Logger::raw(std::string_view fmt, ...) {
    std::lock_guard<std::mutex> lock(mMutex);

    char buffer[kBufferSize];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt.data(), args);
    va_end(args);

    buffer[sizeof(buffer) - 1] = '\0';

    if (mHandler) {
        mHandler(buffer);
    }
    if (mFile != nullptr) {
        std::fputs(buffer, mFile);
        std::fflush(mFile);
    }
}

constexpr const char* Logger::levelToString(Level level) {
    switch (level) {
        case Level::Debug: return "DEBUG";
        case Level::Info:  return "INFO ";
        case Level::Warn:  return "WARN ";
        case Level::Error: return "ERROR";
    }
    return "UNKNOWN";
}

Logger::Level Logger::levelFromString(std::string_view str) {
    if (str == "debug") return Level::Debug;
    if (str == "info")  return Level::Info;
    if (str == "warn")  return Level::Warn;
    if (str == "error") return Level::Error;
    return Level::Info;
}

} // namespace emulator