#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "emulator/cpu/cpu.h"
#include "emulator/utils/singleton.h"

class Terminal;

class Debugger : public Singleton<Debugger> {
public:
  Debugger();
  ~Debugger();

  void run(bool interactive);

  bool processCommand(const std::string &command);

private:
  struct CommandEntry {
    std::string name;
    std::string help;
    bool (Debugger::*Handler)(std::istringstream &);
  };

  std::vector<CommandEntry> mCommands;
  void registerCommands();

  void runPlainInputLoop();

  void setDefaultLogHandler();
  void setTerminalLogHandler();

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

  void updateStatusDisplay();

  std::string formatTrace(const CommitInfo &commit);

  std::function<void(const std::string &)> mOnInput;
  std::function<void()> mOnRun;
  std::function<void(uint32_t)> mOnStep;
  std::function<void()> mOnPause;

  std::mutex mMutex;

  bool mIsInteractive = false;
  std::unique_ptr<Terminal> mTerminal;
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
