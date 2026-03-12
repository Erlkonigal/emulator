#include "emulator/app.h"
#include "emulator/device/ram.h"
#include "emulator/device/rom.h"
#include "emulator/device/uart.h"
#include "emulator/commit/commit_queue.h"
#include "emulator/commit/commit_thread.h"
#include "emulator/commit/shadow_arch.h"
#include "emulator/cpu/cpu.h"
#include "emulator/cpu/cpu_thread.h"
#include "emulator/debug/breakpoint.h"
#include "emulator/debug/debugger.h"
#include "emulator/log/logger.h"
#include "emulator/log/trace_manager.h"
#include "emulator/runtime_config.h"
#include "emulator/debug/input/network_input_handler.h"
#include "emulator/generated/hardware_config.h"
#include "emulator/utils/terminal.h"
#include "emulator/utils/utils.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace emulator {

void EmulatorReset() {
  CpuThread::getInstance().reset();
  CommitThread::getInstance().reset();
  CommitQueue::getInstance().reset();
  ShadowArch::getInstance().init();
  BreakPointController::getInstance().reset();
  Debugger::getInstance().reset();
  RuntimeConfig::getInstance().reset();
  Ram::getInstance().reset();
  Uart::getInstance().reset();
}

int RunEmulator(int argc, char **argv) {
  EmulatorReset();
  auto &config = RuntimeConfig::getInstance();
  bool configRequired = false;
  std::string error;
  if (!config.findConfigPath(argc, argv, &configRequired, &error)) {
    ERROR("%s", error.c_str());
    return 1;
  }
  if (config.showHelp) {
    RuntimeConfig::printUsage(argv[0]);
    return 0;
  }
  if (configRequired) {
    if (!config.loadFromFile(config.configPath, &error)) {
      ERROR("%s", error.c_str());
      return 1;
    }
  }
  if (!config.parseArgs(argc, argv, &error)) {
    ERROR("%s", error.c_str());
    return 1;
  }
  if (config.showHelp) {
    RuntimeConfig::printUsage(argv[0]);
    return 0;
  }

  Logger::Config logConfig;
  logConfig.level = Logger::levelFromString(config.logLevel);
  logConfig.filePath = config.logFile;
  Logger::getInstance().init(logConfig);

  TraceManager::getInstance().setTraceFile(config.traceFile);
  CommitThread::getInstance().init();

  for (const auto& traceName : config.traceOn) {
      TraceManager::getInstance().setEnabled(traceName, true);
  }

  if (config.romPath.empty()) {
    ERROR("ROM path is required");
    RuntimeConfig::printUsage(argv[0]);
    return 1;
  }

  uint64_t romSize = 0;
  if (!getFileSize(config.romPath, &romSize) || romSize == 0) {
    ERROR("failed to read ROM file size");
    return 1;
  }

  std::ifstream romFile(config.romPath, std::ios::binary);
  if (!romFile) {
    ERROR("failed to open ROM file: %s", config.romPath.c_str());
    return 1;
  }

  std::vector<uint8_t> romData(romSize);
  romFile.read(reinterpret_cast<char *>(romData.data()), romSize);
  if (!romFile) {
    ERROR("failed to read ROM file");
    return 1;
  }

  auto& rom = Rom::getInstance();
  rom.init(romData);

  auto& ram = Ram::getInstance();
  ram.init(config.ramSize);

  (void)Uart::getInstance();

  auto cpu = createCpuExecutor();
  if (!cpu) {
    ERROR("Failed to create CPU");
    return 1;
  }

  Debugger::getInstance().setControlCallbacks(
      [&]() { return CommitThread::getInstance().run(); },
      [&](uint32_t count) { return CommitThread::getInstance().step(count); },
      [&]() { return CommitThread::getInstance().pause(); },
      [&]() { NetworkInputHandler::getInstance().requestStop(); return true; });

  Terminal::getInstance().setup(!config.debug);

  CpuThread::getInstance().init(cpu);
  if (!CpuThread::getInstance().start()) {
    ERROR("Failed to start CpuThread");
    return 1;
  }
  if (!CommitThread::getInstance().start()) {
    ERROR("Failed to start CommitThread");
    return 1;
  }

  auto runEventLoop = [&]<typename CheckFunc>(CheckFunc&& shouldContinue) {
    while (shouldContinue() && !Terminal::getInstance().wasInterrupted()) {
      Terminal::getInstance().processIo();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return Terminal::getInstance().wasInterrupted();
  };

  if (config.debug) {
    INFO("Debug mode: starting debug server on port %u", config.debugPort);
    NetworkInputHandler::getInstance().start(config.debugPort);
    bool interrupted = runEventLoop([]{ return NetworkInputHandler::getInstance().isRunning(); });
    if (interrupted) {
      INFO("Interrupted, stopping...");
    }
    NetworkInputHandler::getInstance().stop();
  } else {
    INFO("Running emulator (Ctrl+C to exit)");
    if (!CommitThread::getInstance().run()) {
      ERROR("Failed to run CommitThread");
    }
    bool interrupted = runEventLoop([]{
      auto state = CommitThread::getInstance().getState();
      return state != CommitThreadState::Halted && state != CommitThreadState::Init;
    });
    if (interrupted) {
      INFO("Interrupted, exiting...");
    }
  }

  CpuThread::getInstance().stop();
  CommitThread::getInstance().stop();
  Terminal::getInstance().processIo();
  Terminal::getInstance().restore();

  return Debugger::getInstance().hadError();
}

} // namespace emulator