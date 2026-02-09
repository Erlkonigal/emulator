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

LogLevel ParseLogLevel(const std::string& levelStr) {
    std::string s = ToLower(levelStr);
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
    if (!FindConfigPath(argc, argv, &config, &configRequired, &error)) {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }
    if (config.ShowHelp) {
        PrintUsage(argv[0]);
        return 0;
    }
    if (!LoadConfigFile(config.ConfigPath, configRequired, &config, &error)) {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }
    if (!ParseArgs(argc, argv, &config, &error)) {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }
    if (config.ShowHelp) {
        PrintUsage(argv[0]);
        return 0;
    }

    // Initialize Logging
    LogConfig logConfig;
    logConfig.Level = ParseLogLevel(config.LogLevel);
    if (config.EnableLog) {
        if (config.LogFilename.empty()) {
            // Enable dual output to stdout/stderr
            logConfig.DeviceOutput = "stdout";
            logConfig.LogOutput = "stderr";
        } else {
            // Enable dual output to separate files: name.out and name.err
            logConfig.DeviceOutput = config.LogFilename + ".out";
            logConfig.LogOutput = config.LogFilename + ".err";
        }
    } else {
        logConfig.LogOutput = config.LogFilename;
    }
    LogInit(logConfig);

    if (config.RomPath.empty()) {
        std::fprintf(stderr, "error: ROM path is required\n");
        PrintUsage(argv[0]);
        return 1;
    }
    if (config.RomBase != kDefaultRomBase) {
        std::fprintf(stderr, "error: ROM base must be 0x00000000\n");
        return 1;
    }
    if (config.Width == 0 || config.Height == 0) {
        std::fprintf(stderr, "error: SDL width/height must be non-zero\n");
        return 1;
    }

    uint64_t romSize = 0;
    if (!GetFileSize(config.RomPath, &romSize) || romSize == 0) {
        std::fprintf(stderr, "error: failed to read ROM file size\n");
        return 1;
    }
    uint64_t fbSize = 0;
    if (!ComputeFramebufferSize(config.Width, config.Height, &fbSize)) {
        std::fprintf(stderr, "error: invalid SDL size\n");
        return 1;
    }
    uint64_t sdlSize = SdlDisplayDevice::kControlRegionSize + fbSize;
    if (sdlSize < fbSize) {
        std::fprintf(stderr, "error: SDL mapping size overflow\n");
        return 1;
    }

    std::vector<MemoryRegion> mappings = {
        {"ROM", config.RomBase, romSize},
        {"UART", config.UartBase, kUartSize},
        {"TIMER", config.TimerBase, kTimerSize},
        {"SDL", config.SdlBase, sdlSize},
        {"RAM", config.RamBase, config.RamSize},
    };
    if (!ValidateMappings(mappings, &error)) {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }

    MemoryDevice rom(romSize, true);
    if (!rom.LoadImage(config.RomPath)) {
        std::fprintf(stderr, "error: failed to load ROM image\n");
        return 1;
    }
    MemoryDevice ram(config.RamSize, false);
    UartDevice uart;
    TimerDevice timer;

    MemoryBus bus;
    bus.RegisterDevice(&rom, config.RomBase, romSize, "ROM");
    bus.RegisterDevice(&uart, config.UartBase, kUartSize, "UART");
    bus.RegisterDevice(&timer, config.TimerBase, kTimerSize, "TIMER");

    SdlDisplayDevice sdl;
    if (!sdl.Init(config.Width, config.Height, config.WindowTitle.c_str())) {
        std::fprintf(stderr, "error: SDL initialization failed\n");
        return 1;
    }
    bus.RegisterDevice(&sdl, config.SdlBase, sdl.GetMappedSize(), "SDL");
    bus.RegisterDevice(&ram, config.RamBase, config.RamSize, "RAM");

    ICpuExecutor* cpu = CreateCpuExecutor();
    if (cpu == nullptr) {
        std::fprintf(stderr, "error: CreateCpuExecutor returned null\n");
        return 1;
    }

    // Initialize Debugger as the main execution controller
    Debugger debugger(cpu, &bus);
    debugger.SetRegisterCount(cpu->GetRegisterCount());
    debugger.SetCpuFrequency(config.CpuFrequency);
    debugger.SetSdl(&sdl);

    // Configure Trace through Debugger
    TraceOptions traceOpts;
    traceOpts.LogInstruction = config.ITrace;
    traceOpts.LogMemEvents = config.MTrace;
    traceOpts.LogBranchPrediction = config.BPTrace;
    debugger.ConfigureTrace(traceOpts);

    bus.SetDebugger(&debugger);
    cpu->SetDebugger(&debugger);
    cpu->Reset();
    cpu->SetPc(config.RomBase);

    // Run execution (blocking)
    debugger.Run(config.Debug);

    return 0;
}

