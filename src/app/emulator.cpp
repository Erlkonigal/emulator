#include "emulator/app/app.h"
#include "emulator/app/utils.h"
#include "emulator/logging/logger.h"
#include "emulator/debugger/debugger.h"

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

#include "emulator/bus/bus.h"
#include "emulator/cpu/cpu.h"
#include "emulator/device/device.h"
#include "emulator/device/memory.h"
#include "emulator/device/timer.h"
#include "emulator/device/uart.h"
#include "emulator/device/display.h"

extern "C" ICpuExecutor* CreateCpuExecutor();

int RunEmulator(int argc, char** argv) {
    EmulatorConfig config;
    bool configRequired = false;
    std::string error;
    if (!findConfigPath(argc, argv, &config, &configRequired, &error)) {
        ERROR("%s", error.c_str());
        return 1;
    }
    if (config.showHelp) {
        printUsage(argv[0]);
        return 0;
    }
    if (!loadConfigFile(config.configPath, configRequired, &config, &error)) {
        ERROR("%s", error.c_str());
        return 1;
    }
    if (!parseArgs(argc, argv, &config, &error)) {
        ERROR("%s", error.c_str());
        return 1;
    }
    if (config.showHelp) {
        printUsage(argv[0]);
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
        printUsage(argv[0]);
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

    MemoryDevice rom(romSize, true);
    if (!rom.loadImage(config.romPath)) {
        ERROR("failed to load ROM image");
        return 1;
    }
    MemoryDevice ram(config.ramSize, false);
    UartDevice uart;
    TimerDevice timer;

    MemoryBus bus;
    bus.registerDevice(&rom, config.romBase, romSize, "ROM");
    bus.registerDevice(&uart, config.uartBase, kUartSize, "UART");
    bus.registerDevice(&timer, config.timerBase, kTimerSize, "TIMER");

    SdlDisplayDevice sdl;
    if (config.headless) {
        if (!sdl.initHeadless(config.width, config.height)) {
            ERROR("SDL headless initialization failed");
            return 1;
        }
    } else {
        if (!sdl.init(config.width, config.height, config.windowTitle.c_str())) {
            ERROR("SDL initialization failed");
            return 1;
        }
    }
    bus.registerDevice(&sdl, config.sdlBase, sdl.getMappedSize(), "SDL");
    bus.registerDevice(&ram, config.ramBase, config.ramSize, "RAM");

    ICpuExecutor* cpu = CreateCpuExecutor();
    if (cpu == nullptr) {
        ERROR("CreateCpuExecutor returned null");
        return 1;
    }

    Debugger debugger(cpu, &bus);
    debugger.setRegisterCount(cpu->getRegisterCount());
    debugger.setCpuFrequency(config.cpuFrequency);
    debugger.setSdl(&sdl);

    TraceOptions traceOpts;
    traceOpts.logInstruction = config.iTrace;
    traceOpts.logMemEvents = config.mTrace;
    traceOpts.logBranchPrediction = config.bpTrace;
    debugger.configureTrace(traceOpts);

    bus.setDebugger(&debugger);
    cpu->setDebugger(&debugger);
    cpu->reset();
    cpu->setPc(config.romBase);

    debugger.run(config.debug);

    return !(cpu->getLastError().type == CpuErrorType::None);
}
