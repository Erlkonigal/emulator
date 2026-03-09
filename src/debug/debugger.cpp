#include "emulator/debug/breakpoint.h"
#include "emulator/debug/debugger.h"
#include "emulator/commit/commit_thread.h"
#include "emulator/commit/shadow_arch.h"
#include "emulator/debug/expression_parser.h"
#include "emulator/log/logger.h"
#include "emulator/log/tracer.h"
#include "emulator/generated/hardware_config.h"
#include "emulator/utils/utils.h"

#include <cctype>
#include <chrono>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

void Debugger::reset() {
  BreakPointController::getInstance().reset();
  mHadError.store(false, std::memory_order_release);
  mCpuRunning.store(false, std::memory_order_release);
  mQuitRequested.store(false, std::memory_order_release);
}

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
      {"trace", "Toggle tracer (trace on|off [itrace])",
       &Debugger::cmdTrace},
      {"help", "Show this help message", &Debugger::cmdHelp}};
}

void Debugger::processCommand(const std::string &command) {
  std::string trimmed = command;
  trimInPlace(&trimmed);
  if (trimmed.empty()) {
    return;
  }

  std::istringstream stream(trimmed);
  std::string verb;
  stream >> verb;

  for (const auto &cmd : mCommands) {
    if (cmd.name == verb) {
      (this->*cmd.Handler)(stream);
      return;
    }
  }
  
  LINE("Unknown command: %s", verb.c_str());
}

void Debugger::cmdRun(std::istringstream &args) {
  (void)args;
  auto& commitThread = CommitThread::getInstance();
  if (commitThread.wasStarted() && commitThread.getState() == CommitThreadState::Halted) {
    LINE("CPU is halted. Cannot run.");
    return;
  }
  if (mOnRun) {
    mOnRun();
    mCpuRunning.store(true, std::memory_order_release);
  }
}

void Debugger::cmdStep(std::istringstream &args) {
  auto& commitThread = CommitThread::getInstance();
  if (commitThread.wasStarted() && commitThread.getState() == CommitThreadState::Halted) {
    LINE("CPU is halted. Cannot step.");
    return;
  }
  uint32_t count = 1;
  args >> count;
  if (mOnStep) {
    mOnStep(count);
  }
}

void Debugger::cmdPause(std::istringstream &args) {
  (void)args;
  auto& commitThread = CommitThread::getInstance();
  if (commitThread.wasStarted() && commitThread.getState() == CommitThreadState::Halted) {
    LINE("CPU is halted. Cannot pause.");
    return;
  }
  if (mOnPause) {
    mOnPause();
    mCpuRunning.store(false, std::memory_order_release);
  }
}

void Debugger::cmdQuit(std::istringstream &args) {
  (void)args;
  mQuitRequested.store(true, std::memory_order_release);
  if (mOnQuit) {
    mOnQuit();
  }
}

void Debugger::cmdRegs(std::istringstream &args) {
  (void)args;
  auto &shadowArch = ShadowArch::getInstance();
  for (uint32_t regId = 0; regId < kMaxNumRegisters; ++regId) {
    uint64_t val = shadowArch.readReg(regId);
    LINE("r%u = 0x%llx", regId, (unsigned long long)val);
  }
}

void Debugger::cmdMem(std::istringstream &args) {
  std::string addrStr;
  std::string lenStr;
  args >> addrStr >> lenStr;
  if (addrStr.empty() || lenStr.empty()) {
    LINE("Usage: mem <addr> <len>");
    return;
  }

  uint64_t addr = 0;
  uint64_t len = 0;
  if (!parseU64(addrStr, &addr) || !parseU64(lenStr, &len)) {
    LINE("Invalid address or length");
    return;
  }

  auto &shadowArch = ShadowArch::getInstance();
  std::string line;
  for (uint64_t i = 0; i < len; ++i) {
    if (i % 16 == 0) {
      if (!line.empty()) {
        LINE("%s", line.c_str());
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
    LINE("%s", line.c_str());
  }
}

void Debugger::cmdEval(std::istringstream &args) {
  std::string expr;
  std::getline(args, expr);
  trimInPlace(&expr);
  if (expr.empty()) {
    return;
  }
  uint64_t value = 0;
  if (!parseU64(expr, &value)) {
    LINE("Invalid expression");
    return;
  }
  LINE("0x%llx (%llu)", (unsigned long long)value, (unsigned long long)value);
}

void Debugger::cmdBp(std::istringstream &args) {
  std::string action;
  args >> action;

  if (action == "list" || action.empty()) {
    std::vector<std::string> lines;
    BreakPointController::getInstance().list(lines);
    if (lines.empty()) {
      LINE("No breakpoints.");
    } else {
      LINE("Breakpoints:");
      for (const auto &line : lines) {
        LINE("  %s", line.c_str());
      }
    }
    return;
  }

  std::string addrStr;
  args >> addrStr;
  uint64_t addr = 0;
  if (!parseU64(addrStr, &addr)) {
    LINE("Invalid address");
    return;
  }

  if (action == "add") {
    addBreakpoint(addr);
    LINE("Breakpoint added at 0x%llx", (unsigned long long)addr);
    return;
  }
  if (action == "del") {
    removeBreakpoint(addr);
    LINE("Breakpoint removed at 0x%llx", (unsigned long long)addr);
    return;
  }

  LINE("Usage: bp list|add <addr>|del <addr>");
}

void Debugger::cmdLog(std::istringstream &args) {
  std::string levelStr;
  args >> levelStr;
  std::string trimmed = toLower(levelStr);
  trimInPlace(&trimmed);

  Logger::Level level = Logger::Level::Info;
  bool valid = false;

  if (trimmed == "debug") {
    level = Logger::Level::Debug;
    valid = true;
  } else if (trimmed == "info") {
    level = Logger::Level::Info;
    valid = true;
  } else if (trimmed == "warn") {
    level = Logger::Level::Warn;
    valid = true;
  } else if (trimmed == "error") {
    level = Logger::Level::Error;
    valid = true;
  }

  if (valid) {
    Logger::getInstance().setLevel(level);
    LINE("Log level set to %s", levelStr.c_str());
    return;
  }

  LINE("Usage: log [trace|debug|info|warn|error]");
}

void Debugger::cmdTrace(std::istringstream &args) {
  std::string action;
  args >> action;
  
  if (action != "on" && action != "off") {
    LINE("Usage: trace on|off [itrace]");
    return;
  }
  
  std::string traceName;
  args >> traceName;
  if (traceName.empty()) {
    traceName = "itrace";
  }
  
  if (traceName == "itrace") {
    CommitThread::getInstance().iTrace.setEnabled(action == "on");
    LINE("ITRACE %s", action == "on" ? "enabled" : "disabled");
    return;
  }
  
  LINE("Unknown tracer: %s", traceName.c_str());
}

void Debugger::cmdHelp(std::istringstream &args) {
  (void)args;
  LINE("Available commands:");

  size_t maxNameLen = 0;
  for (const auto &cmd : mCommands) {
    if (cmd.name.length() > maxNameLen) {
      maxNameLen = cmd.name.length();
    }
  }

  for (const auto &cmd : mCommands) {
    std::string padding(maxNameLen - cmd.name.length() + 2, ' ');
    LINE("  %s%s%s", cmd.name.c_str(), padding.c_str(), cmd.help.c_str());
  }
}

void Debugger::addBreakpoint(uint64_t address) {
  BreakPointController::getInstance().add(address);
}

void Debugger::removeBreakpoint(uint64_t address) {
  BreakPointController::getInstance().remove(address);
}

void Debugger::runPlainInputLoop() {
  std::string line;
  std::cout << "dbg> " << std::flush;
  while (std::getline(std::cin, line)) {
    if (line == "quit" || line == "exit") {
      break;
    }
    processCommand(line);
    std::cout << "dbg> " << std::flush;
  }
}