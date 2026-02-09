#include "emulator/logging/logging.h"

#include <cstdio>
#include <mutex>
#include <ctime>
#include <vector>

namespace {

struct LogOutput {
    FILE* File = nullptr;
    bool OwnsFile = false;
    
    void Close() {
        if (OwnsFile && File != nullptr && File != stdout && File != stderr) {
            std::fclose(File);
        }
        File = nullptr;
        OwnsFile = false;
    }
    
    void Open(const std::string& filename, FILE* defaultFile) {
        Close();
        if (filename.empty()) {
            File = defaultFile;
            OwnsFile = false;
        } else if (filename == "stdout") {
            File = stdout;
            OwnsFile = false;
        } else if (filename == "stderr") {
            File = stderr;
            OwnsFile = false;
        } else {
            File = std::fopen(filename.c_str(), "w");
            if (File) {
                OwnsFile = true;
            } else {
                std::fprintf(stderr, "Failed to open log file: %s\n", filename.c_str());
                File = defaultFile;
            }
        }
    }
};

class LogState {
public:
    std::mutex Mutex;
    LogOutput Device;  // For device output (UART, etc.) - like stdout
    LogOutput Log;     // For log messages - like stderr
    LogLevel Level = LogLevel::Info;
    std::function<void(const char*)> OutputHandler;
    bool DualMode = false;
    
    void Reset() {
        Device.Close();
        Log.Close();
        DualMode = false;
    }
    
    void Initialize(const LogConfig& config) {
        std::lock_guard<std::mutex> lock(Mutex);
        Reset();
        
        Level = config.Level;
        DualMode = config.IsDualOutput();
        
        // If no specific outputs configured, both go to stderr (original behavior)
        if (!DualMode) {
            if (config.LogOutput.empty()) {
                Device.Open("", stdout);
                Log.Open("", stderr);
            } else {
                // Both outputs go to the same file
                Device.Open(config.LogOutput, stdout);
                Log.File = Device.File;
                Log.OwnsFile = false;  // Only one owner
            }
        } else {
            // Dual mode: separate outputs
            Device.Open(config.DeviceOutput, stdout);
            Log.Open(config.LogOutput, stderr);
        }
    }
    
    void Write(FILE* file, const char* str) {
        if (OutputHandler) {
            OutputHandler(str);
        } else if (file) {
            std::fprintf(file, "%s", str);
            std::fflush(file);
        }
    }
    
    void WriteLine(FILE* file, const char* str) {
        if (OutputHandler) {
            OutputHandler((std::string(str) + "\n").c_str());
        } else if (file) {
            std::fprintf(file, "%s\n", str);
            std::fflush(file);
        }
    }
};

LogState g_State;

} // namespace

void LogInit(const LogConfig& config) {
    g_State.Initialize(config);
}

void LogSetLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(g_State.Mutex);
    g_State.Level = level;
}

void LogSetOutputHandler(std::function<void(const char*)> handler) {
    std::lock_guard<std::mutex> lock(g_State.Mutex);
    g_State.OutputHandler = std::move(handler);
}

void LogMessage(LogLevel level, const char* file, int line, const char* fmt, ...) {
    if (level < g_State.Level) return;

    std::lock_guard<std::mutex> lock(g_State.Mutex);
    
    // Time
    std::time_t now = std::time(nullptr);
    char timeBuf[64];
    std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", std::localtime(&now));

    // Level
    const char* levelStr = "INFO";
    switch (level) {
        case LogLevel::Trace: levelStr = "TRACE"; break;
        case LogLevel::Debug: levelStr = "DEBUG"; break;
        case LogLevel::Info:  levelStr = "INFO "; break;
        case LogLevel::Warn:  levelStr = "WARN "; break;
        case LogLevel::Error: levelStr = "ERROR"; break;
    }

    // Determine short filename
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

    g_State.WriteLine(g_State.Log.File, buffer);
}

void LogPrint(const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(g_State.Mutex);

    char buffer[4096];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    buffer[sizeof(buffer) - 1] = '\0';

    g_State.Write(g_State.Log.File, buffer);
}

void LogDevicePrint(const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(g_State.Mutex);

    char buffer[4096];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    buffer[sizeof(buffer) - 1] = '\0';

    g_State.Write(g_State.Device.File, buffer);
}
