#include "emulator/app/app.h"
#include "emulator/app/utils.h"
#include "emulator/logging/logging.h"
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

namespace {

LogLevel parseLogLevel(const std::string& levelStr) {
    std::string s = toLower(levelStr);
    if (s == "trace") return LogLevel::Trace;
    if (s == "debug") return LogLevel::Debug;
    if (s == "info") return LogLevel::Info;
    if (s == "warn") return LogLevel::Warn;
    if (s == "error") return LogLevel::Error;
    return LogLevel::Info;
}

}

int RunEmulator(int argc, char** argv) {
    EmulatorConfig config;
    bool configRequired = false;
    std::string error;
    if (!findConfigPath(argc, argv, &config, &configRequired, &error)) {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }
    if (config.showHelp) {
        printUsage(argv[0]);
        return 0;
    }
    if (!loadConfigFile(config.configPath, configRequired, &config, &error)) {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }
    if (!parseArgs(argc, argv, &config, &error)) {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }
    if (config.showHelp) {
        printUsage(argv[0]);
        return 0;
    }

    LogConfig logConfig;
    logConfig.level = parseLogLevel(config.logLevel);
    if (config.enableLog) {
        if (config.logFilename.empty()) {
            logConfig.deviceOutput = "stdout";
            logConfig.logOutput = "stderr";
        } else {
            logConfig.deviceOutput = config.logFilename + ".out";
            logConfig.logOutput = config.logFilename + ".err";
        }
    } else {
        logConfig.logOutput = config.logFilename;
    }
    logInit(logConfig);

    if (config.romPath.empty()) {
        std::fprintf(stderr, "error: ROM path is required\n");
        printUsage(argv[0]);
        return 1;
    }
    if (config.romBase != kDefaultRomBase) {
        std::fprintf(stderr, "error: ROM base must be 0x00000000\n");
        return 1;
    }
    if (config.width == 0 || config.height == 0) {
        std::fprintf(stderr, "error: SDL width/height must be non-zero\n");
        return 1;
    }

    uint64_t romSize = 0;
    if (!getFileSize(config.romPath, &romSize) || romSize == 0) {
        std::fprintf(stderr, "error: failed to read ROM file size\n");
        return 1;
    }
    uint64_t fbSize = 0;
    if (!computeFramebufferSize(config.width, config.height, &fbSize)) {
        std::fprintf(stderr, "error: invalid SDL size\n");
        return 1;
    }
    uint64_t sdlSize = SdlDisplayDevice::kControlRegionSize + fbSize;
    if (sdlSize < fbSize) {
        std::fprintf(stderr, "error: SDL mapping size overflow\n");
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
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }

    MemoryDevice rom(romSize, true);
    if (!rom.loadImage(config.romPath)) {
        std::fprintf(stderr, "error: failed to load ROM image\n");
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
            std::fprintf(stderr, "error: SDL headless initialization failed\n");
            return 1;
        }
    } else {
        if (!sdl.init(config.width, config.height, config.windowTitle.c_str())) {
            std::fprintf(stderr, "error: SDL initialization failed\n");
            return 1;
        }
    }
    bus.registerDevice(&sdl, config.sdlBase, sdl.getMappedSize(), "SDL");
    bus.registerDevice(&ram, config.ramBase, config.ramSize, "RAM");

    ICpuExecutor* cpu = CreateCpuExecutor();
    if (cpu == nullptr) {
        std::fprintf(stderr, "error: CreateCpuExecutor returned null\n");
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
