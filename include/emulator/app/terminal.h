#pragma once

#include "emulator/app/vterm_manager.h"
#include "emulator/logging/logger.h"

#include <string>
#include <mutex>
#include <functional>
#include <atomic>
#include <queue>
#include <termios.h>
#include <cstring>

enum class FocusPanel {
    VTERM,
    DEBUG
};

class Terminal {
public:
    Terminal();
    ~Terminal();

    void printLog(const char* msg);
    void printChar(uint8_t ch);
    void updateStatus(const std::string& status);

    using OnCommandCallback = std::function<void(const std::string&)>;
    void setOnCommand(OnCommandCallback cb) { mOnCommand = cb; }

    VTermManager& getVTermManager() { return mVTermManager; }
    FocusPanel getFocus() const { return mFocus; }
    void switchFocus();

    void handleMouse(int y, int x);
    void runCursesInputLoop();
    void stop() { mShouldClose = true; }
    void setOnInput(std::function<void(const std::string&)> callback) { mOnInput = callback; }

private:
    void renderAll();
    void processDebugInput(int ch);
    void setupWindows();
    void handleCursesResize();

    std::mutex mMutex;

    WINDOW* mStatusWin = nullptr;
    WINDOW* mVtermBorder = nullptr;
    WINDOW* mDebugWin = nullptr;

    VTermManager mVTermManager;
    FocusPanel mFocus = FocusPanel::VTERM;

    std::string mCurrentStatus;
    bool mLastCmdSuccess = true;

    std::string mDebugInput;
    int mDebugCursorPos = 0;
    int mHeight = 0;

    OnCommandCallback mOnCommand;
    std::atomic<bool> mShouldClose{false};
    
    std::function<void(const std::string&)> mOnInput;
};


class TermiosGuard {
    int mFd;
    struct termios mOriginalSettings;
    bool mValid;

public:
    explicit TermiosGuard(int fd, struct termios& newSettings)
        : mFd(fd), mOriginalSettings{}, mValid(true) {
        if (!isatty(fd)) {
            mValid = false;
            return;
        }
        if (tcgetattr(fd, &mOriginalSettings) != 0) {
            ERROR("Failed to get terminal attributes: %s", strerror(errno));
            mValid = false;
            return;
        }
        if (tcsetattr(fd, TCSANOW, &newSettings) != 0) {
            ERROR("Failed to set terminal attributes: %s", strerror(errno));
            mValid = false;
        }
    }

    ~TermiosGuard() {
        if (mValid) {
            tcsetattr(mFd, TCSANOW, &mOriginalSettings);
        }
    }

    TermiosGuard(const TermiosGuard&) = delete;
    TermiosGuard& operator=(const TermiosGuard&) = delete;

    bool isValid() const noexcept { return mValid; }
};