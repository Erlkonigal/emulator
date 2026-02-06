#include "emulator/app.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <thread>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <termios.h>
#include <unistd.h>
#endif

#include "emulator/bus.h"
#include "emulator/cpu.h"
#include "emulator/device.h"
#include "emulator/trace.h"

extern "C" ICpuExecutor* CreateCpuExecutor();

namespace {
#if defined(__unix__) || defined(__APPLE__)
class TerminalEchoGuard {
public:
    TerminalEchoGuard() = default;
    ~TerminalEchoGuard() {
        Restore();
    }

    void DisableEcho() {
        if (Active) {
            return;
        }
        if (!isatty(STDIN_FILENO)) {
            return;
        }
        struct termios current;
        if (tcgetattr(STDIN_FILENO, &current) != 0) {
            return;
        }
        Original = current;
        current.c_lflag &= static_cast<tcflag_t>(~ECHO);
        if (tcsetattr(STDIN_FILENO, TCSANOW, &current) != 0) {
            return;
        }
        Active = true;
    }

    void Restore() {
        if (!Active) {
            return;
        }
        tcsetattr(STDIN_FILENO, TCSANOW, &Original);
        Active = false;
    }

private:
    bool Active = false;
    struct termios Original = {};
};
#else
class TerminalEchoGuard {
public:
    void DisableEcho() {}
};
#endif
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
    bus.RegisterDevice(&rom, config.RomBase, romSize);
    bus.RegisterDevice(&uart, config.UartBase, kUartSize);
    bus.RegisterDevice(&timer, config.TimerBase, kTimerSize);

    SdlDisplayDevice sdl;
    if (!sdl.Init(config.Width, config.Height, config.WindowTitle.c_str())) {
        std::fprintf(stderr, "error: SDL initialization failed\n");
        return 1;
    }
    bus.RegisterDevice(&sdl, config.SdlBase, sdl.GetMappedSize());
    bus.RegisterDevice(&ram, config.RamBase, config.RamSize);

    ICpuExecutor* cpu = CreateCpuExecutor();
    if (cpu == nullptr) {
        std::fprintf(stderr, "error: CreateCpuExecutor returned null\n");
        return 1;
    }
    TraceSink trace;
    cpu->SetMemoryBus(&bus);
    cpu->SetTraceSink(&trace);
    cpu->Reset();
    cpu->SetPc(config.RomBase);

    EmulatorRunState runState;
    runState.State.store(config.Debug ? CpuState::Pause : CpuState::Running,
        std::memory_order_release);
    CpuControl control;

    std::thread cpuThread(RunCpuThread, cpu, &timer, &runState, &control);
    std::thread sdlThread(RunSdlThread, &sdl, &runState, &control);

    if (config.Debug) {
        RunDebugger(cpu, &bus, &uart, &runState, &control);
    } else {
        TerminalEchoGuard echoGuard;
        echoGuard.DisableEcho();
        while (runState.State.load(std::memory_order_acquire) == CpuState::Running) {
            PumpTerminalInput(&uart);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    runState.State.store(CpuState::Finish, std::memory_order_release);
    control.Cv.notify_all();
    if (cpuThread.joinable()) {
        cpuThread.join();
    }
    if (sdlThread.joinable()) {
        sdlThread.join();
    }

    sdl.Shutdown();
    return 0;
}
