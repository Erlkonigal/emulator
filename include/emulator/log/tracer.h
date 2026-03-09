#pragma once

#include <cstdio>
#include <functional>
#include <mutex>
#include <string>

class Tracer {
public:
    struct Config {
        std::string name;
        bool enabled = false;
        std::string filePath;
        std::function<void(const char*)> handler;
    };

    Tracer() = default;
    ~Tracer();

    void init(const Config& config);
    void setEnabled(bool enabled);
    void setHandler(std::function<void(const char*)> handler);
    bool isEnabled() const;
    void trace(const char* fmt, ...);

private:
    std::mutex mMutex;
    std::string mName;
    bool mEnabled = false;
    std::string mFilePath;
    FILE* mFile = nullptr;
    std::function<void(const char*)> mHandler;
};