#pragma once

#include <cstdio>
#include <functional>
#include <mutex>
#include <string>

class Tracer {
public:
    struct Config {
        std::string name;
        std::function<void(const char*)> handler;
    };

    Tracer() = default;
    virtual ~Tracer();

    void init(const Config& config);
    void setHandler(std::function<void(const char*)> handler);
    const std::string& name() const { return mName; }
    virtual void trace(const char* fmt, ...);

protected:
    std::mutex mMutex;
    std::string mName;
    FILE* mFile = nullptr;
    std::function<void(const char*)> mHandler;
};