#include "emulator/app.h"
#include "emulator/commit/commit_queue.h"
#include "emulator/commit/commit_thread.h"
#include "emulator/cpu/cpu.h"
#include "emulator/cpu/cpu_thread.h"
#include "emulator/debug/breakpoint.h"
#include "emulator/debug/debugger.h"
#include "emulator/log/logger.h"
#include "emulator/utils/config.h"
#include "emulator/utils/utils.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

int RunEmulator(int argc, char **argv) {
  EmulatorConfig config;
  bool configRequired = false;
  std::string error;
  if (!EmulatorConfig::findConfigPath(argc, argv, &config, &configRequired,
                                      &error)) {
    ERROR("%s", error.c_str());
    return 1;
  }
  if (config.showHelp) {
    EmulatorConfig::printUsage(argv[0]);
    return 0;
  }
  if (!EmulatorConfig::loadConfigFile(config.configPath, configRequired,
                                      &config, &error)) {
    ERROR("%s", error.c_str());
    return 1;
  }
  if (!EmulatorConfig::parseArgs(argc, argv, &config, &error)) {
    ERROR("%s", error.c_str());
    return 1;
  }
  if (config.showHelp) {
    EmulatorConfig::printUsage(argv[0]);
    return 0;
  }

  logging::Config logConfig;
  logConfig.level = logging::levelFromString(config.logLevel);
  if (!config.logFilename.empty()) {
    logConfig.mFile = config.logFilename;
  }
  logging::init(logConfig);

  if (config.romPath.empty()) {
    ERROR("ROM path is required");
    EmulatorConfig::printUsage(argv[0]);
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

  auto cpu = createCpuExecutor(romData);
  if (!cpu) {
    ERROR("Failed to create CPU");
    return 1;
  }

  Debugger::getInstance().configureTrace(&config);

  Debugger::getInstance().setControlCallbacks(
      [&]() { CommitThread::getInstance().run(); },
      [&](uint32_t count) { CommitThread::getInstance().step(count); },
      [&]() { CommitThread::getInstance().pause(); });

  CpuThread::getInstance().init(cpu);
  CpuThread::getInstance().start();
  CommitThread::getInstance().start();

  Debugger::getInstance().run(config.debug);

  CpuThread::getInstance().stop();
  CommitThread::getInstance().stop();

  return Debugger::getInstance().hadError() ? 1 : 0;
}