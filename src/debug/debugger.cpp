#include "emulator/debug/debugger.h"
#include "emulator/app.h"
#include "emulator/control/control.h"
#include "emulator/cpu/cpu_thread.h"
#include "emulator/debug/commit_thread.h"
#include "emulator/debug/expression_parser.h"
#include "emulator/device/device.h"
#include "emulator/device/display.h"
#include "emulator/device/sdl_thread.h"
#include "emulator/device/uart.h"
#include "emulator/log/logger.h"
#include "emulator/utils/utils.h"

#include "emulator/terminal/terminal.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <poll.h>
#include <sstream>
#include <termios.h>
#include <thread>
#include <unistd.h>

namespace {} // namespace

// Unused constants removed

Debugger::Debugger(std::shared_ptr<Controller> controller)
    : mController(controller) {
  registerCommands();
}

Debugger::~Debugger() {
  if (mIsInteractive) {
    setDefaultLogHandler();
  }
}

void Debugger::run(bool interactive) {
  mIsInteractive = interactive;
  if (!mIsInteractive) {
    runPlainInputLoop();
    return;
  }

  mTerminal = std::make_unique<Terminal>();
  setTerminalLogHandler();
  mTerminal->setOnCommand([this](const std::string &command) {
    mLastCommandSuccess = processCommand(command);
    updateStatusDisplay();
  });

  mTerminal->runCursesInputLoop();

  // Reset logging after terminal stops
  setDefaultLogHandler();
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

void Debugger::setCpuFrequency(uint32_t cpuFreq) {
  mCpuFrequency = cpuFreq;
  // Sync threshold logic moved to emulator or bus
}

void Debugger::configureTrace(const EmulatorConfig *config) {
  if (!config)
    return;
  mEnableITrace = config->iTrace;
  mEnableMTrace = config->mTrace;
  mEnableBpTrace = config->bpTrace;
}

void Debugger::initDbgArchFromCpu() {
  // Can no longer init from CPU directly.
  // We'll rely on the first commits to populate state.
  mDbgArch.pc = 0;
  mDbgArch.cycle = 0;
}

void Debugger::updateDbgArch(const CommitInfo &commit) {
  mDbgArch.pc = commit.pc;

  if (commit.regWrite && commit.regId < mDbgArch.regs.size()) {
    mDbgArch.regs[commit.regId] = commit.regData;
  }

  if (commit.memWrite) {
    // Assume 4-byte write for now as commit.memData is uint32_t
    for (uint32_t i = 0; i < 4; ++i) {
      uint64_t addr = commit.memAddress + i;
      mDbgArch.mem.bytes[addr] =
          static_cast<uint8_t>((commit.memData >> (i * 8)) & 0xff);
    }
  }

  mDbgArch.cycle++;
}

void Debugger::processCommits(const std::vector<CommitInfo> &commits) {
  for (const auto &commit : commits) {
    if (commit.errorType != CpuErrorType::None) {
      if (commit.errorType != CpuErrorType::Halt) {
        // Only report error if we were still running.
        // This avoids overwriting success if the CPU thread runs a few more
        // cycles after Halt and encounters a fetch error (common in tests where
        // code ends exactly at Halt).
        if (mCpuRunning.load(std::memory_order_acquire)) {
          mHadError.store(true, std::memory_order_release);
        }
      }
      mCpuRunning.store(false, std::memory_order_release);
      // Do not pause consumer here to avoid deadlock during specific test
      // scenarios
    }
    updateDbgArch(commit);
    mTotalInstructions++;

    if (mEnableITrace || mEnableMTrace || mEnableBpTrace) {
      std::string line = formatTrace(commit);
      if (!line.empty()) {
        TRACE("%s", line.c_str());
      }
    }
  }
}

void Debugger::flushTraceBatch() {
  std::vector<CommitInfo> commits;
  size_t count = mController->popCommits(commits, 1024);
  if (count == 0)
    return;
  if (!(mEnableITrace || mEnableMTrace || mEnableBpTrace))
    return;

  for (const auto &commit : commits) {
    std::string line = formatTrace(commit);
    if (!line.empty()) {
      TRACE("%s", line.c_str());
    }
  }
}

void Debugger::flushTrace() { flushTraceBatch(); }

std::string Debugger::formatTrace(const CommitInfo &commit) {
  std::stringstream ss;

  if (mEnableITrace) {
    ss << "PC:0x" << std::hex << std::setw(8) << std::setfill('0') << commit.pc
       << " ";
    ss << "Inst:0x" << std::hex << std::setw(8) << std::setfill('0')
       << commit.inst << " ";
    // decoded, isBranch, etc not in CommitInfo currently
    ss << " ";
  }

  if (mEnableMTrace && (commit.memWrite)) {
    ss << "Mem:[";
    ss << "W:0x" << std::hex << commit.memAddress << "=" << commit.memData;
    ss << "]";
    ss << " ";
  }

  return ss.str();
}

void Debugger::setSdl(std::shared_ptr<SdlDisplayDevice> sdl) { mSdl = sdl; }

void Debugger::setRegisterCount(uint32_t count) { mRegisterCount = count; }

std::vector<uint8_t> Debugger::scanMemory(uint64_t address, uint32_t length) {
  std::vector<uint8_t> data(length, 0);
  for (uint32_t i = 0; i < length; ++i) {
    auto it = mDbgArch.mem.bytes.find(address + i);
    if (it != mDbgArch.mem.bytes.end()) {
      data[i] = it->second;
    }
  }
  return data;
}

// ... existing code ...

std::vector<uint64_t> Debugger::readRegisters() {
  std::vector<uint64_t> regs;
  if (mRegisterCount == 0) {
    return regs; // Need reg count from first commit
  }
  regs.resize(mRegisterCount);
  for (uint32_t regId = 0; regId < mRegisterCount; ++regId) {
    if (regId < getDbgArch().regs.size()) {
      regs[regId] = getDbgArch().regs[regId];
    }
  }
  return regs;
}

void Debugger::printRegisters() {
  std::vector<uint64_t> regs = readRegisters();
  for (uint32_t regId = 0; regId < regs.size(); ++regId) {
    INFO("r%u = 0x%llx", regId, (unsigned long long)regs[regId]);
  }
}

uint64_t Debugger::evalExpression(const std::string &expression) {
  if (expression.empty())
    return 0;
  ExpressionParser parser(this, expression);
  return parser.parse();
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

bool Debugger::isBreakpoint(uint64_t address) {
  std::lock_guard<std::mutex> lock(mMutex);
  return std::find(mBreakpoints.begin(), mBreakpoints.end(), address) !=
         mBreakpoints.end();
}

bool Debugger::hasBreakpoints() {
  std::lock_guard<std::mutex> lock(mMutex);
  return !mBreakpoints.empty();
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
  }
  return true;
}

bool Debugger::cmdStep(std::istringstream &args) {
  if (mHadError.load(std::memory_order_acquire)) {
    INFO("CPU is halted. Cannot step.");
    return false;
  }
  uint32_t count = 1;
  if (args >> count) {
    // count successfully read
  } else {
    count = 1;
  }
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
  }
  return true;
}

bool Debugger::cmdQuit(std::istringstream &args) {
  (void)args;
  if (mTerminal) {
    mTerminal->stop();
  }
  return true;
}

bool Debugger::cmdRegs(std::istringstream &args) {
  (void)args;
  printRegisters();
  return true;
}

bool Debugger::cmdMem(std::istringstream &args) {
  std::string addrStr;
  std::string lenStr;
  args >> addrStr >> lenStr;
  if (!addrStr.empty() && !lenStr.empty()) {
    uint64_t addr = evalExpression(addrStr);
    uint64_t len = evalExpression(lenStr);
    std::vector<uint8_t> data = scanMemory(addr, static_cast<uint32_t>(len));

    std::string line;
    for (size_t i = 0; i < data.size(); ++i) {
      if (i % 16 == 0) {
        char header[32];
        snprintf(header, sizeof(header),
                 "%08llx: ", (unsigned long long)(addr + i));
        line += header;
      }

      char byteStr[8];
      snprintf(byteStr, sizeof(byteStr), "%02x ", data[i]);
      line += byteStr;

      if (i % 16 == 15 || i + 1 == data.size()) {
        INFO("%s", line.c_str());
        line.clear();
      }
    }
    return true;
  }
  return false;
}

bool Debugger::cmdEval(std::istringstream &args) {
  std::string expr;
  std::getline(args, expr);
  if (!expr.empty()) {
    uint64_t value = evalExpression(expr);
    INFO("0x%llx (%llu)", (unsigned long long)value, (unsigned long long)value);
    return true;
  }
  return false;
}

bool Debugger::cmdBp(std::istringstream &args) {
  std::string action;
  std::string addrStr;
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

  args >> addrStr;
  if (action == "add" && !addrStr.empty()) {
    addBreakpoint(evalExpression(addrStr));
    return true;
  }
  if (action == "del" && !addrStr.empty()) {
    removeBreakpoint(evalExpression(addrStr));
    return true;
  }
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

void Debugger::updateStatusDisplay() {
  if (!mTerminal)
    return;

  std::string stateStr;
  if (mHadError.load(std::memory_order_acquire)) {
    stateStr = "HALTED ";
  } else {
    stateStr = "PAUSED ";
  }

  uint64_t cycles = mDbgArch.cycle;
  uint64_t instrs = mTotalInstructions;
  uint64_t pc = mDbgArch.pc;

  char buffer[256];

  double ipc = 0.0;
  if (cycles > 0) {
    ipc = static_cast<double>(instrs) / static_cast<double>(cycles);
  }

  double cps = 0;
  auto now = std::chrono::steady_clock::now();
  double dt = std::chrono::duration<double>(now - mLastCpsTime).count();
  if (dt > 0) {
    double dCycles = static_cast<double>(cycles - mLastCpsCycles);
    cps = dCycles / dt;
  }
  mLastCpsTime = now;
  mLastCpsCycles = cycles;

  char cpsBuf[32];
  if (cps >= 1000000.0) {
    snprintf(cpsBuf, sizeof(cpsBuf), "%.2fM", cps / 1000000.0);
  } else if (cps >= 1000.0) {
    snprintf(cpsBuf, sizeof(cpsBuf), "%.2fK", cps / 1000.0);
  } else {
    snprintf(cpsBuf, sizeof(cpsBuf), "%.0f", cps);
  }

  const char *cmdStatus = mLastCommandSuccess ? "OK" : "ERR";

  snprintf(buffer, sizeof(buffer),
           "CPU: %s | PC: 0x%llx | Cycles: %llu | Instrs: %llu | IPC: %.2f | "
           "CPS: %s | LAST: %s",
           stateStr.c_str(), (unsigned long long)pc, (unsigned long long)cycles,
           (unsigned long long)instrs, ipc, cpsBuf, cmdStatus);

  mTerminal->updateStatus(buffer);
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

void Debugger::setDefaultLogHandler() {
  logging::setOutputHandler(nullptr, nullptr);
}

void Debugger::setTerminalLogHandler() {
  if (!mTerminal)
    return;
  logging::setOutputHandler(
      [this](const char *msg) {
        if (mTerminal)
          mTerminal->printLog(msg);
      },
      [this](const char *msg) {
        if (mTerminal)
          mTerminal->printLog(msg);
      });
}
