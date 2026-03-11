#pragma once

#include <atomic>
#include <functional>
#include <string>

#include "emulator/debug/debug_context.h"
#include "emulator/utils/singleton.h"

class Debugger : public Singleton<Debugger> {
public:
    void reset();
    bool hadError() const { return mHadError.load(); }

    void processCommand(const std::string& command);

    void setControlCallbacks(std::function<bool()> onRun,
                              std::function<bool(uint32_t)> onStep,
                              std::function<bool()> onPause,
                              std::function<bool()> onQuit = nullptr) {
        mContext.setControlCallbacks(std::move(onRun), std::move(onStep),
                                      std::move(onPause), std::move(onQuit));
    }

    void setOutputHandler(std::function<void(const std::string&)> handler) {
        mContext.setOutputHandler(std::move(handler));
    }

    bool wasQuitRequested() const { return mContext.wasQuitRequested(); }
    DebuggerState state() const { return mContext.state(); }

    DebugContext& context() { return mContext; }

private:
    DebugContext mContext;
    std::atomic<bool> mHadError{false};

    Debugger();
    friend class Singleton<Debugger>;
};