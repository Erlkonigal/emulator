#include "emulator/logging/logger.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <functional>
#include <mutex>
#include <string>

namespace {

constexpr size_t kBufferSize = 4096;

struct Output {
    FILE* mFile = nullptr;
    bool mOwnsFile = false;
    std::function<void(const char*)> mHandler;

    void close() {
        if (mOwnsFile && mFile != nullptr && mFile != stdout && mFile != stderr) {
            std::fclose(mFile);
        }
        mFile = nullptr;
        mOwnsFile = false;
    }

    void open(const std::string& filename) {
        if (filename.empty()) {
            mFile = nullptr;
            mOwnsFile = false;
        } else if (filename == "stdout") {
            mFile = stdout;
            mOwnsFile = false;
        } else if (filename == "stderr") {
            mFile = stderr;
            mOwnsFile = false;
        } else {
            mFile = std::fopen(filename.c_str(), "w");
            if (mFile) {
                mOwnsFile = true;
            } else {
                std::fprintf(stderr, "Failed to open log file: %s\n", filename.c_str());
                mFile = nullptr;
            }
        }
    }

    void write(const char* str) {
        if (mHandler) {
            mHandler(str);
        }
        if (mFile && mOwnsFile) {
            std::fprintf(mFile, "%s", str);
            std::fflush(mFile);
        }
    }
};

class Backend {
public:
    std::mutex mMutex;
    Output mDevice;
    Output mLog;
    logging::Level mLevel = logging::Level::Info;

    void reset() {
        mDevice.close();
        mLog.close();
    }

    void initialize(const logging::Config& config) {
        std::lock_guard<std::mutex> lock(mMutex);
        reset();

        mLevel = config.level;

        mDevice.open(config.mDeviceFile);
        mLog.open(config.mFile);

        mLog.mHandler = config.mOnMessage;
        mDevice.mHandler = config.mOnDeviceMessage;
    }
};

Backend gBackend;

const char* levelToString(logging::Level level) {
    switch (level) {
        case logging::Level::Trace: return "TRACE";
        case logging::Level::Debug: return "DEBUG";
        case logging::Level::Info:  return "INFO ";
        case logging::Level::Warn:  return "WARN ";
        case logging::Level::Error: return "ERROR";
    }
    return "UNKNOWN";
}

const char* extractFilename(const char* path) {
    const char* result = path;
    for (const char* p = path; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            result = p + 1;
        }
    }
    return result;
}

void formatMessage(logging::Level level, const char* file, int line,
                    const char* fmt, va_list args) {
    if (level < gBackend.mLevel) return;

    std::lock_guard<std::mutex> lock(gBackend.mMutex);

    time_t now = time(nullptr);
    char timeBuf[64];
    strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", localtime(&now));

    char buffer[kBufferSize];
    int pos = std::snprintf(buffer, sizeof(buffer),
                            "[%s] [%s] %s:%d: ",
                            timeBuf, ::levelToString(level),
                            extractFilename(file), line);


    std::string lineFmt = std::string(fmt) + "\n";
    if (pos > 0 && static_cast<size_t>(pos) < sizeof(buffer)) {
        std::vsnprintf(buffer + pos, sizeof(buffer) - pos, lineFmt.c_str(), args);
    }

    buffer[sizeof(buffer) - 1] = '\0';
    gBackend.mLog.write(buffer);
}

} // namespace

namespace logging {

void init(const Config& config) {
    gBackend.initialize(config);
}

void level(Level newLevel) {
    std::lock_guard<std::mutex> lock(gBackend.mMutex);
    gBackend.mLevel = newLevel;
}

void setOutputHandler(std::function<void(const char*)> logHandler,
                        std::function<void(const char*)> deviceHandler) {
    std::lock_guard<std::mutex> lock(gBackend.mMutex);
    gBackend.mLog.mHandler = std::move(logHandler);
    gBackend.mDevice.mHandler = std::move(deviceHandler);
}

void info(const char* file, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    formatMessage(Level::Info, file, line, fmt, args);
    va_end(args);
}

void debug(const char* file, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    formatMessage(Level::Debug, file, line, fmt, args);
    va_end(args);
}

void warn(const char* file, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    formatMessage(Level::Warn, file, line, fmt, args);
    va_end(args);
}

void error(const char* file, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    formatMessage(Level::Error, file, line, fmt, args);
    va_end(args);
}

void trace(const char* file, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    formatMessage(Level::Trace, file, line, fmt, args);
    va_end(args);
}

void raw(const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(gBackend.mMutex);

    char buffer[kBufferSize];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    buffer[sizeof(buffer) - 1] = '\0';
    gBackend.mLog.write(buffer);
}

void device(const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(gBackend.mMutex);

    char buffer[kBufferSize];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    buffer[sizeof(buffer) - 1] = '\0';
    gBackend.mDevice.write(buffer);
}

const char* levelToString(Level level) {
    return ::levelToString(level);
}

Level levelFromString(const std::string& str) {
    std::string s = str;
    for (auto& c : s) c = static_cast<char>(tolower(c));

    if (s == "trace") return Level::Trace;
    if (s == "debug") return Level::Debug;
    if (s == "info")  return Level::Info;
    if (s == "warn")  return Level::Warn;
    if (s == "error") return Level::Error;
    return Level::Info;
}

} // namespace logging
