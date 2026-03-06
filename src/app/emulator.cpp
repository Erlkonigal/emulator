#include "emulator/app.h"
#include "emulator/debug/debugger.h"
#include "emulator/log/logger.h"
#include "emulator/utils/config.h"
#include "emulator/utils/utils.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

#include "emulator/cpu/cpu.h"
#include "emulator/device/bus.h"
#include "emulator/device/device.h"
#include "emulator/device/display.h"
#include "emulator/device/memory.h"
#include "emulator/device/timer.h"
#include "emulator/device/uart.h"

#include "emulator/cpu/cpu_thread.h"
#include "emulator/debug/commit_thread.h"
#include "emulator/device/sdl_thread.h"

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
    logConfig.mDeviceFile = config.logFilename + ".out";
    logConfig.mFile = config.logFilename + ".err";
  }
  logging::init(logConfig);

  if (config.romPath.empty()) {
    ERROR("ROM path is required");
    EmulatorConfig::printUsage(argv[0]);
    return 1;
  }
  if (config.romBase != kDefaultRomBase) {
    ERROR("ROM base must be 0x00000000");
    return 1;
  }
  if (config.width == 0 || config.height == 0) {
    ERROR("SDL width/height must be non-zero");
    return 1;
  }

  uint64_t romSize = 0;
  if (!getFileSize(config.romPath, &romSize) || romSize == 0) {
    ERROR("failed to read ROM file size");
    return 1;
  }
  uint64_t fbSize = 0;
  if (!computeFramebufferSize(config.width, config.height, &fbSize)) {
    ERROR("invalid SDL size");
    return 1;
  }
  uint64_t sdlSize = SdlDisplayDevice::kControlRegionSize + fbSize;
  if (sdlSize < fbSize) {
    ERROR("SDL mapping size overflow");
    return 1;
  }

  std::vector<MemoryRegion> mappings = {
      {"ROM", config.romBase, romSize},
      {"UART", config.uartBase, kUartSize},
      {"TIMER", config.timerBase, kTimerSize},
      {"SDL", config.sdlBase, sdlSize},
      {"RAM", config.ramBase, config.ramSize},
  };
  if (!validateMappings(mappings, &error)) {
    ERROR("%s", error.c_str());
    return 1;
  }

  auto rom = std::make_shared<MemoryDevice>(romSize, true);
  if (!rom->loadImage(config.romPath)) {
    ERROR("failed to load ROM image");
    return 1;
  }
  auto ram = std::make_shared<MemoryDevice>(config.ramSize, false);
  auto uart = std::make_shared<UartDevice>();
  auto timer = std::make_shared<TimerDevice>();

  // extern "C" removed

  // ...

  auto bus = std::make_shared<MemoryBus>();
  bus->registerDevice(rom, config.romBase, romSize, "ROM");
  bus->registerDevice(uart, config.uartBase, kUartSize, "UART");
  bus->registerDevice(timer, config.timerBase, kTimerSize, "TIMER");

  auto sdl = std::make_shared<SdlDisplayDevice>();
  if (config.headless) {
    if (!sdl->initHeadless(config.width, config.height)) {
      ERROR("SDL headless initialization failed");
      return 1;
    }
  } else {
    if (!sdl->init(config.width, config.height, config.windowTitle.c_str())) {
      ERROR("SDL initialization failed");
      return 1;
    }
  }
  bus->registerDevice(sdl, config.sdlBase, sdl->getMappedSize(), "SDL");
  bus->registerDevice(ram, config.ramBase, config.ramSize, "RAM");

  std::shared_ptr<ICpuExecutor> cpu =
      createCpuExecutor(bus); // cpu is shared_ptr
  if (cpu == nullptr) {
    ERROR("createCpuExecutor returned null");
    return 1;
  }

  auto controller = std::make_shared<Controller>();
  // Default queue sizes used (1024 commit, 64 csr)

  auto debugger = std::make_shared<Debugger>(controller);
  debugger->setRegisterCount(cpu->getRegisterCount());
  debugger->setCpuFrequency(config.cpuFrequency);
  debugger->setSdl(sdl);
  debugger->configureTrace(&config);

  // Wire up control callbacks
  auto commitThread = std::make_shared<CommitThread>(controller, debugger);
  commitThread->setCpuFrequency(config.cpuFrequency);

  debugger->setControlCallbacks(
      [commitThread]() { commitThread->resume(); },
      [commitThread](uint32_t count) { commitThread->step(count); },
      [commitThread]() { commitThread->pause(); });

  debugger->setOnInput([uart](const std::string &data) {
    if (uart) {
      for (char c : data) {
        uart->pushRx(static_cast<uint8_t>(c));
      }
    }
  });

  // bus->setDebugger(debugger.get()); // If bus still needs debugger, but we
  // decoupled it? Actually, MemoryBus doesn't need debugger anymore for
  // syncAll, it's called by CommitThread

  cpu->reset();
  cpu->setResetPc(config.romBase);

  // Simulation threads
  auto cpuThread = std::make_unique<CpuThread>(cpu, controller);
  auto sdlThread = std::make_unique<SdlThread>(sdl);

  cpuThread->start();
  commitThread->start();
  if (!config.debug) {
    commitThread->resume();
  }
  if (!config.headless) {
    sdlThread->start();
  }

  // Run the debugger CLI (blocks until quit)
  debugger->run(config.debug);

  // Shutdown sequence
  controller->cancel();
  cpuThread->stop();
  cpuThread->wait();

  if (sdlThread->isRunning()) {
    sdlThread->stop();
    sdlThread->wait();
  }
  commitThread->stop();

  if (sdl) {
    sdl->shutdown();
  }

  return debugger->hadError();
}
