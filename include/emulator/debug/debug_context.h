#pragma once

#include <atomic>
#include <cstdarg>
#include <functional>
#include <string>
#include <vector>

#include "emulator/debug/debug_state.h"

class DebugContext {
public:
    void reset();

    DebuggerState state() const { return mState.load(std::memory_order_acquire); }
    void setState(DebuggerState s) { mState.store(s, std::memory_order_release); }

    void output(const char* fmt, ...);
    void outputLine(const std::string& line);

    void setOutputHandler(std::function<void(const std::string&)> handler) {
        mOutputHandler = std::move(handler);
    }

    void setControlCallbacks(std::function<bool()> onRun,
                              std::function<bool(uint32_t)> onStep,
                              std::function<bool()> onPause,
                              std::function<bool()> onQuit) {
        mOnRun = std::move(onRun);
        mOnStep = std::move(onStep);
        mOnPause = std::move(onPause);
        mOnQuit = std::move(onQuit);
    }

    bool run();
    bool step(uint32_t count);
    bool pause();
    bool quit();
    bool wasQuitRequested() const { return mQuitRequested.load(); }

    uint64_t readReg(uint32_t id);
    uint8_t readMem(uint64_t addr);
    uint64_t readPc();

    void addBreakpoint(uint64_t addr);
    void removeBreakpoint(uint64_t addr);
    bool hasBreakpoint(uint64_t addr);
    std::vector<std::string> listBreakpoints();

    void setLogLevel(int level);
    void setTraceEnabled(const std::string& name, bool enabled);

private:
    std::atomic<DebuggerState> mState{DebuggerState::Idle};
    std::atomic<bool> mQuitRequested{false};

    std::function<bool()> mOnRun;
    std::function<bool(uint32_t)> mOnStep;
    std::function<bool()> mOnPause;
    std::function<bool()> mOnQuit;
    std::function<void(const std::string&)> mOutputHandler;
};