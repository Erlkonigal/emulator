#include "emulator/log/tracer.h"

#include <cstdarg>
#include <cstring>
#include <ctime>

namespace {

constexpr size_t kBufferSize = 4096;

}

Tracer::~Tracer() {
    if (mFile != nullptr && mFile != stderr) {
        std::fclose(mFile);
    }
}

void Tracer::init(const Config& config) {
    std::lock_guard<std::mutex> lock(mMutex);
    
    mName = config.name;
    mEnabled = config.enabled;
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

void Tracer::setEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(mMutex);
    mEnabled = enabled;
}

void Tracer::setHandler(std::function<void(const char*)> handler) {
    std::lock_guard<std::mutex> lock(mMutex);
    mHandler = std::move(handler);
}

bool Tracer::isEnabled() const {
    return mEnabled;
}

void Tracer::trace(const char* fmt, ...) {
    if (!mEnabled) return;

    std::lock_guard<std::mutex> lock(mMutex);
    
    if (!mEnabled) return;

    time_t now = std::time(nullptr);
    struct tm tmBuf;
    localtime_r(&now, &tmBuf);

    char timeBuf[64];
    std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tmBuf);

    char buffer[kBufferSize];
    int pos = std::snprintf(buffer, sizeof(buffer), "[%s] [%s] ", timeBuf, mName.c_str());

    if (pos > 0 && static_cast<size_t>(pos) < sizeof(buffer)) {
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(buffer + pos, sizeof(buffer) - pos, fmt, args);
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