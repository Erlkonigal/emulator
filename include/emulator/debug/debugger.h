#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

#include "emulator/utils/singleton.h"

class Debugger : public Singleton<Debugger> {
public:
  void run(bool interactive);
  void reset();
  bool hadError() const { return mHadError.load(); }

  void processCommand(const std::string &command);

void setControlCallbacks(std::function<void()> onRun,
                            std::function<void(uint32_t)> onStep,
                            std::function<void()> onPause,
                            std::function<void()> onQuit = nullptr) {
    mOnRun = std::move(onRun);
    mOnStep = std::move(onStep);
    mOnPause = std::move(onPause);
    mOnQuit = std::move(onQuit);
  }

  bool wasQuitRequested() const { return mQuitRequested.load(); }

  void addBreakpoint(uint64_t address);
  void removeBreakpoint(uint64_t address);

private:
  struct CommandEntry {
    std::string name;
    std::string help;
    void (Debugger::*Handler)(std::istringstream &);
  };

  std::vector<CommandEntry> mCommands;
  void registerCommands();

  void runPlainInputLoop();

  void cmdRun(std::istringstream &args);
  void cmdStep(std::istringstream &args);
  void cmdPause(std::istringstream &args);
  void cmdQuit(std::istringstream &args);
  void cmdRegs(std::istringstream &args);
  void cmdMem(std::istringstream &args);
  void cmdEval(std::istringstream &args);
  void cmdBp(std::istringstream &args);
  void cmdLog(std::istringstream &args);
  void cmdTrace(std::istringstream &args);
  void cmdHelp(std::istringstream &args);

  std::function<void()> mOnRun;
  std::function<void(uint32_t)> mOnStep;
  std::function<void()> mOnPause;
  std::function<void()> mOnQuit;

  bool mIsInteractive = false;
  uint64_t mTotalInstructions = 0;

  std::chrono::steady_clock::time_point mLastCpsTime;
  uint64_t mLastCpsCycles = 0;

  std::atomic<bool> mHadError{false};
  std::atomic<bool> mCpuRunning{false};
  std::atomic<bool> mQuitRequested{false};

private:
  Debugger() { registerCommands(); }
  friend class Singleton<Debugger>;
};