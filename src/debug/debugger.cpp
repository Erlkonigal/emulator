#include "emulator/debug/debugger.h"
#include "emulator/commit/commit_thread.h"
#include "emulator/commit/shadow_arch.h"
#include "emulator/debug/expression_parser.h"
#include "emulator/log/logger.h"
#include "emulator/utils/config.h"
#include "emulator/utils/utils.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

Debugger::Debugger() { registerCommands(); }

Debugger::~Debugger() = default;

void Debugger::run(bool interactive) {
  mIsInteractive = interactive;
  if (!interactive) {
    if (mOnRun) {
      mOnRun();
      mCpuRunning.store(true, std::memory_order_release);
      
      auto &commitThread = CommitThread::getInstance();
      while (commitThread.getState() != CommitThreadState::Halted &&
             commitThread.getState() != CommitThreadState::Paused) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      mCpuRunning.store(false, std::memory_order_release);
    }
    return;
  }
  runPlainInputLoop();
}

void Debugger::registerCommands() {
  mCommands = {
      {"run", "Resume execution", &Debugger::cmdRun},
      {"step", "Execute N instructions (default 1)", &Debugger::cmdStep},
      {"pause", "Pause execution", &Debugger::cmdPause},
      {"quit", "Exit the emulator", &Debugger::cmdQuit},
      {"exit", "Exit the emulator", &Debugger::cmdQuit},
      {"regs", "Print register values", &Debugger::cmdRegs},
      {"mem", "Dump memory (mem <addr> <len>)", &Debugger::cmdMem},
      {"eval", "Evaluate an expression (eval <expr>)", &Debugger::cmdEval},
      {"bp", "Manage breakpoints (bp list|add <addr>|del <addr>)",
       &Debugger::cmdBp},
      {"log", "Set log level (log trace|debug|info|warn|error)",
       &Debugger::cmdLog},
      {"help", "Show this help message", &Debugger::cmdHelp}};
}

void Debugger::configureTrace(const EmulatorConfig *config) {
  if (!config)
    return;
  mEnableITrace = config->iTrace;
  mEnableMTrace = config->mTrace;
  mEnableBpTrace = config->bpTrace;
}

bool Debugger::processCommand(const std::string &command) {
  std::string trimmed = command;
  trimInPlace(&trimmed);
  if (trimmed.empty()) {
    return true;
  }

  std::istringstream stream(trimmed);
  std::string verb;
  stream >> verb;

  for (const auto &cmd : mCommands) {
    if (cmd.name == verb) {
      return (this->*cmd.Handler)(stream);
    }
  }

  return false;
}

bool Debugger::cmdRun(std::istringstream &args) {
  (void)args;
  if (mHadError.load(std::memory_order_acquire)) {
    INFO("CPU is halted. Cannot run.");
    return false;
  }
  if (mOnRun) {
    mOnRun();
    mCpuRunning.store(true, std::memory_order_release);
  }
  return true;
}

bool Debugger::cmdStep(std::istringstream &args) {
  if (mHadError.load(std::memory_order_acquire)) {
    INFO("CPU is halted. Cannot step.");
    return false;
  }
  uint32_t count = 1;
  args >> count;
  if (mOnStep) {
    mOnStep(count);
  }
  return true;
}

bool Debugger::cmdPause(std::istringstream &args) {
  (void)args;
  if (mHadError.load(std::memory_order_acquire)) {
    INFO("CPU is halted. Cannot pause.");
    return false;
  }
  if (mOnPause) {
    mOnPause();
    mCpuRunning.store(false, std::memory_order_release);
  }
  return true;
}

bool Debugger::cmdQuit(std::istringstream &args) {
  (void)args;
  return true;
}

bool Debugger::cmdRegs(std::istringstream &args) {
  (void)args;
  auto &shadowArch = ShadowArch::getInstance();
  for (uint32_t regId = 0; regId < kMaxNumRegisters; ++regId) {
    uint64_t val = shadowArch.readReg(regId);
    INFO("r%u = 0x%llx", regId, (unsigned long long)val);
  }
  return true;
}

bool Debugger::cmdMem(std::istringstream &args) {
  std::string addrStr;
  std::string lenStr;
  args >> addrStr >> lenStr;
  if (addrStr.empty() || lenStr.empty()) {
    INFO("Usage: mem <addr> <len>");
    return false;
  }

  uint64_t addr = 0;
  uint64_t len = 0;
  if (!parseU64(addrStr, &addr) || !parseU64(lenStr, &len)) {
    INFO("Invalid address or length");
    return false;
  }

  auto &shadowArch = ShadowArch::getInstance();
  std::string line;
  for (uint64_t i = 0; i < len; ++i) {
    if (i % 16 == 0) {
      if (!line.empty()) {
        INFO("%s", line.c_str());
      }
      char header[32];
      snprintf(header, sizeof(header), "%08llx: ",
               (unsigned long long)(addr + i));
      line = header;
    }

    uint8_t byte = shadowArch.readMem(addr + i);
    char byteStr[8];
    snprintf(byteStr, sizeof(byteStr), "%02x ", byte);
    line += byteStr;
  }
  if (!line.empty()) {
    INFO("%s", line.c_str());
  }
  return true;
}

bool Debugger::cmdEval(std::istringstream &args) {
  std::string expr;
  std::getline(args, expr);
  trimInPlace(&expr);
  if (expr.empty()) {
    return false;
  }
  // Simple expression evaluation - just parse hex/decimal numbers for now
  uint64_t value = 0;
  if (!parseU64(expr, &value)) {
    INFO("Invalid expression");
    return false;
  }
  INFO("0x%llx (%llu)", (unsigned long long)value, (unsigned long long)value);
  return true;
}

bool Debugger::cmdBp(std::istringstream &args) {
  std::string action;
  args >> action;

  if (action == "list" || action.empty()) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mBreakpoints.empty()) {
      INFO("No breakpoints.");
    } else {
      INFO("Breakpoints:");
      for (uint64_t bp : mBreakpoints) {
        INFO("  0x%llx", (unsigned long long)bp);
      }
    }
    return true;
  }

  std::string addrStr;
  args >> addrStr;
  uint64_t addr = 0;
  if (!parseU64(addrStr, &addr)) {
    INFO("Invalid address");
    return false;
  }

  if (action == "add") {
    addBreakpoint(addr);
    INFO("Breakpoint added at 0x%llx", (unsigned long long)addr);
    return true;
  }
  if (action == "del") {
    removeBreakpoint(addr);
    INFO("Breakpoint removed at 0x%llx", (unsigned long long)addr);
    return true;
  }

  INFO("Usage: bp list|add <addr>|del <addr>");
  return false;
}

bool Debugger::cmdLog(std::istringstream &args) {
  std::string levelStr;
  args >> levelStr;
  std::string trimmed = toLower(levelStr);
  trimInPlace(&trimmed);

  logging::Level level = logging::Level::Info;
  bool valid = false;

  if (trimmed == "trace") {
    level = logging::Level::Trace;
    valid = true;
  } else if (trimmed == "debug") {
    level = logging::Level::Debug;
    valid = true;
  } else if (trimmed == "info") {
    level = logging::Level::Info;
    valid = true;
  } else if (trimmed == "warn") {
    level = logging::Level::Warn;
    valid = true;
  } else if (trimmed == "error") {
    level = logging::Level::Error;
    valid = true;
  }

  if (valid) {
    logging::level(level);
    INFO("Log level set to %s", levelStr.c_str());
    return true;
  }

  INFO("Usage: log [trace|debug|info|warn|error]");
  return true;
}

bool Debugger::cmdHelp(std::istringstream &args) {
  (void)args;
  INFO("Available commands:");

  size_t maxNameLen = 0;
  for (const auto &cmd : mCommands) {
    if (cmd.name.length() > maxNameLen) {
      maxNameLen = cmd.name.length();
    }
  }

  for (const auto &cmd : mCommands) {
    std::string padding(maxNameLen - cmd.name.length() + 2, ' ');
    INFO("  %s%s%s", cmd.name.c_str(), padding.c_str(), cmd.help.c_str());
  }
  return true;
}

void Debugger::addBreakpoint(uint64_t address) {
  std::lock_guard<std::mutex> lock(mMutex);
  if (std::find(mBreakpoints.begin(), mBreakpoints.end(), address) ==
      mBreakpoints.end()) {
    mBreakpoints.push_back(address);
  }
}

void Debugger::removeBreakpoint(uint64_t address) {
  std::lock_guard<std::mutex> lock(mMutex);
  mBreakpoints.erase(
      std::remove(mBreakpoints.begin(), mBreakpoints.end(), address),
      mBreakpoints.end());
}

void Debugger::runPlainInputLoop() {
  std::string line;
  std::cout << "dbg> " << std::flush;
  while (std::getline(std::cin, line)) {
    if (line == "quit" || line == "exit") {
      break;
    }
    mLastCommandSuccess = processCommand(line);
    std::cout << "dbg> " << std::flush;
  }
}