#pragma once

#include <atomic>
#include <thread>

class IThread {
public:
    virtual ~IThread() = default;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void reset() = 0;

    bool isRunning() const { return mRunning.load(std::memory_order_acquire); }
    bool wasStarted() const { return mStarted.load(std::memory_order_acquire); }

protected:
    IThread() = default;

    virtual void threadLoop() = 0;

    void setRunning(bool running) { mRunning.store(running, std::memory_order_release); }
    void setStarted(bool started) { mStarted.store(started, std::memory_order_release); }

    void joinThread() {
        if (mThread.joinable()) {
            mThread.join();
        }
    }

    std::thread mThread;
    std::atomic<bool> mRunning{false};
    std::atomic<bool> mStarted{false};
};