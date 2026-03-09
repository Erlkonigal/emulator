#include "emulator/app.h"
#include "emulator/bus/bus.h"
#include "emulator/bus/ram.h"
#include "emulator/bus/rom.h"
#include "emulator/bus/uart.h"
#include "emulator/commit/commit_queue.h"
#include "emulator/commit/commit_thread.h"
#include "emulator/commit/shadow_arch.h"
#include "emulator/cpu/cpu.h"
#include "emulator/cpu/cpu_thread.h"
#include "emulator/debug/breakpoint.h"
#include "emulator/debug/debugger.h"
#include "emulator/log/logger.h"
#include "emulator/runtime_config.h"
#include "emulator/server/debug_server.h"
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

void EmulatorReset() {
  CpuThread::getInstance().reset();
  CommitThread::getInstance().reset();
  CommitQueue::getInstance().reset();
  ShadowArch::getInstance().reset();
  BreakPointController::getInstance().reset();
  Bus::getInstance().clear();
  Debugger::getInstance().reset();
  RuntimeConfig::getInstance().reset();
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

  CommitThread::getInstance().iTrace.init({
      .name = "ITRACE",
      .enabled = config.iTrace,
      .filePath = config.traceFile
  });

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

  auto& bus = Bus::getInstance();

  auto& rom = Rom::getInstance();
  rom.init(kRomBase, romData);
  bus.registerDevice(&rom);

  auto& ram = Ram::getInstance();
  ram.init(kRamBase, config.ramSize);
  bus.registerDevice(&ram);

  auto& uart = Uart::getInstance();
  uart.setBaseAddr(kUartBase);
  bus.registerDevice(&uart);

  auto cpu = createCpuExecutor();
  if (!cpu) {
    ERROR("Failed to create CPU");
    return 1;
  }

  Debugger::getInstance().setControlCallbacks(
      [&]() { CommitThread::getInstance().run(); },
      [&](uint32_t count) { CommitThread::getInstance().step(count); },
      [&]() { CommitThread::getInstance().pause(); },
      [&]() { DebugServer::getInstance().requestStop(); });

  Terminal::getInstance().setup();

  CpuThread::getInstance().init(cpu);
  CpuThread::getInstance().start();
  CommitThread::getInstance().start();

  if (config.debug) {
    INFO("Debug mode: starting debug server on port %u", config.debugPort);
    DebugServer::getInstance().start(config.debugPort);

    while (DebugServer::getInstance().isRunning()) {
      Terminal::getInstance().processIo();
      if (Terminal::getInstance().wasInterrupted()) {
        INFO("Interrupted, stopping...");
        DebugServer::getInstance().stop();
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    DebugServer::getInstance().stop();
  } else {
    INFO("Running emulator (Ctrl+C to exit)");
    CommitThread::getInstance().run();

    while (CommitThread::getInstance().getState() != CommitThreadState::Halted) {
      Terminal::getInstance().processIo();
      if (Terminal::getInstance().wasInterrupted()) {
        INFO("Interrupted, exiting...");
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  CpuThread::getInstance().stop();
  CommitThread::getInstance().stop();
  Terminal::getInstance().processIo();
  Terminal::getInstance().restore();

  return Debugger::getInstance().hadError() ? 1 : 0;
}