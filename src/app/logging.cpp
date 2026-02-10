#include "emulator/logging/logging.h"

#include <cstdio>
#include <mutex>
#include <ctime>
#include <vector>

namespace {

struct LogOutput {
    FILE* file = nullptr;
    bool ownsFile = false;
    
    void close() {
        if (ownsFile && file != nullptr && file != stdout && file != stderr) {
            std::fclose(file);
        }
        file = nullptr;
        ownsFile = false;
    }
    
    void open(const std::string& filename, FILE* defaultFile) {
        close();
        if (filename.empty()) {
            file = defaultFile;
            ownsFile = false;
        } else if (filename == "stdout") {
            file = stdout;
            ownsFile = false;
        } else if (filename == "stderr") {
            file = stderr;
            ownsFile = false;
        } else {
            file = std::fopen(filename.c_str(), "w");
            if (file) {
                ownsFile = true;
            } else {
                std::fprintf(stderr, "Failed to open log file: %s\n", filename.c_str());
                file = defaultFile;
            }
        }
    }
};

class LogState {
public:
    std::mutex mutex;
    LogOutput device;
    LogOutput log;
    LogLevel level = LogLevel::Info;
    std::function<void(const char*)> outputHandler;
    bool dualMode = false;
    
    void reset() {
        device.close();
        log.close();
        dualMode = false;
    }
    
    void initialize(const LogConfig& config) {
        std::lock_guard<std::mutex> lock(mutex);
        reset();
        
        level = config.level;
        dualMode = config.isDualOutput();
        
        if (!dualMode) {
            if (config.logOutput.empty()) {
                device.open("", stdout);
                log.open("", stderr);
            } else {
                device.open(config.logOutput, stdout);
                log.file = device.file;
                log.ownsFile = false;
            }
        } else {
            device.open(config.deviceOutput, stdout);
            log.open(config.logOutput, stderr);
        }
    }
    
    void write(FILE* f, const char* str) {
        if (outputHandler) {
            outputHandler(str);
        } else if (f) {
            std::fprintf(f, "%s", str);
            std::fflush(f);
        }
    }
    
    void writeLine(FILE* f, const char* str) {
        if (outputHandler) {
            outputHandler(str);
        } else if (f) {
            std::fprintf(f, "%s\n", str);
            std::fflush(f);
        }
    }
};

LogState gState;

} // namespace

void logInit(const LogConfig& config) {
    gState.initialize(config);
}

void logSetLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(gState.mutex);
    gState.level = level;
}

void logSetOutputHandler(std::function<void(const char*)> handler) {
    std::lock_guard<std::mutex> lock(gState.mutex);
    gState.outputHandler = std::move(handler);
}

void logMessage(LogLevel level, const char* file, int line, const char* fmt, ...) {
    if (level < gState.level) return;

    std::lock_guard<std::mutex> lock(gState.mutex);
    
    std::time_t now = std::time(nullptr);
    char timeBuf[64];
    std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", std::localtime(&now));

    const char* levelStr = "INFO";
    switch (level) {
        case LogLevel::Trace: levelStr = "TRACE"; break;
        case LogLevel::Debug: levelStr = "DEBUG"; break;
        case LogLevel::Info:  levelStr = "INFO "; break;
        case LogLevel::Warn:  levelStr = "WARN "; break;
        case LogLevel::Error: levelStr = "ERROR"; break;
    }

    const char* shortFile = file;
    for (const char* p = file; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            shortFile = p + 1;
        }
    }

    char buffer[4096];
    int len = std::snprintf(buffer, sizeof(buffer), "[%s] [%s] %s:%d: ", timeBuf, levelStr, shortFile, line);
    
    if (len > 0 && static_cast<size_t>(len) < sizeof(buffer)) {
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(buffer + len, sizeof(buffer) - len, fmt, args);
        va_end(args);
    }
    buffer[sizeof(buffer) - 1] = '\0';

    gState.writeLine(gState.log.file, buffer);
}

void logPrint(const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(gState.mutex);

    char buffer[4096];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    buffer[sizeof(buffer) - 1] = '\0';

    gState.write(gState.log.file, buffer);
}

void logDevicePrint(const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(gState.mutex);

    char buffer[4096];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    buffer[sizeof(buffer) - 1] = '\0';

    gState.write(gState.device.file, buffer);
}
