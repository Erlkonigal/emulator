#pragma once

#include "emulator/log/logger.h"
#include "emulator/terminal/vterm_manager.h"
#include "emulator/utils/singleton.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <termios.h>
#include <thread>

enum class FocusPanel { VTERM, DEBUG };

class Terminal : public Singleton<Terminal> {
public:
  using OnCommandCallback = std::function<void(const std::string &)>;
  using OnInputCallback = std::function<void(const std::string &)>;

  Terminal();
  ~Terminal();

  void init();

  void printLog(const char *msg);
  void printChar(uint8_t ch);
  void updateStatus(const std::string &status);

  FocusPanel getFocus() const { return mFocus; }
  void switchFocus();

  void handleMouse(int y, int x);
  void runCursesInputLoop();
  void stop();

  void setOnCommand(OnCommandCallback cb) { mOnCommand = cb; }
  void setOnInput(OnInputCallback callback) { mOnInput = callback; }

private:
  void renderAll();
  void processDebugInput(int ch);
  void setupWindows();
  void handleCursesResize();

  std::mutex mMutex;

  WINDOW *mStatusWin = nullptr;
  WINDOW *mVtermBorder = nullptr;
  WINDOW *mDebugWin = nullptr;

  VTermManager mVTermManager;
  FocusPanel mFocus = FocusPanel::VTERM;

  std::string mCurrentStatus;
  bool mLastCmdSuccess = true;

  std::string mDebugInput;
  int mDebugCursorPos = 0;
  int mHeight = 0;

  OnCommandCallback mOnCommand;
  std::atomic<bool> mShouldClose{false};

  std::function<void(const std::string &)> mOnInput;

  void startRefreshThread();
  void stopRefreshThread();
  void refreshLoop();

  std::thread mRefreshThread;
  std::atomic<bool> mRefreshRunning{false};

  friend class Singleton<Terminal>;
};

class TermiosGuard {
  int mFd;
  struct termios mOriginalSettings;
  bool mValid;

public:
  explicit TermiosGuard(int fd, struct termios &newSettings)
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

  TermiosGuard(const TermiosGuard &) = delete;
  TermiosGuard &operator=(const TermiosGuard &) = delete;

  bool isValid() const noexcept { return mValid; }
};