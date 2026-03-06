#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include "emulator/utils/singleton.h"

struct EmulatorConfig;

class Debugger : public Singleton<Debugger> {
public:
  Debugger();
  ~Debugger();

  void run(bool interactive);
  bool hadError() const { return mHadError.load(); }

  bool processCommand(const std::string &command);

  void setControlCallbacks(std::function<void()> onRun,
                           std::function<void(uint32_t)> onStep,
                           std::function<void()> onPause) {
    mOnRun = std::move(onRun);
    mOnStep = std::move(onStep);
    mOnPause = std::move(onPause);
  }

  void configureTrace(const EmulatorConfig *config);

  void addBreakpoint(uint64_t address);
  void removeBreakpoint(uint64_t address);

private:
  struct CommandEntry {
    std::string name;
    std::string help;
    bool (Debugger::*Handler)(std::istringstream &);
  };

  std::vector<CommandEntry> mCommands;
  void registerCommands();

  void runPlainInputLoop();

  bool cmdRun(std::istringstream &args);
  bool cmdStep(std::istringstream &args);
  bool cmdPause(std::istringstream &args);
  bool cmdQuit(std::istringstream &args);
  bool cmdRegs(std::istringstream &args);
  bool cmdMem(std::istringstream &args);
  bool cmdEval(std::istringstream &args);
  bool cmdBp(std::istringstream &args);
  bool cmdLog(std::istringstream &args);
  bool cmdHelp(std::istringstream &args);

  std::function<void()> mOnRun;
  std::function<void(uint32_t)> mOnStep;
  std::function<void()> mOnPause;

  std::mutex mMutex;
  std::vector<uint64_t> mBreakpoints;

  bool mIsInteractive = false;
  uint64_t mTotalInstructions = 0;
  bool mLastCommandSuccess = true;

  bool mEnableITrace = false;
  bool mEnableMTrace = false;
  bool mEnableBpTrace = false;

  std::chrono::steady_clock::time_point mLastCpsTime;
  uint64_t mLastCpsCycles = 0;

  std::atomic<bool> mHadError{false};
  std::atomic<bool> mCpuRunning{false};
};