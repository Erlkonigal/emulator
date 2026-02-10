#ifndef EMULATOR_APP_APP_H
#define EMULATOR_APP_APP_H

#include <cstdint>
#include <string>
#include <vector>

#include "emulator/bus/bus.h"
#include "emulator/cpu/cpu.h"
#include "emulator/debugger/debugger.h"

constexpr uint64_t kDefaultRomBase = 0x00000000;
constexpr uint64_t kDefaultRamBase = 0x80000000;
constexpr uint64_t kDefaultRamSize = 256ull * 1024 * 1024;
constexpr uint64_t kDefaultUartBase = 0x20000000;
constexpr uint64_t kDefaultTimerBase = 0x20001000;
constexpr uint64_t kDefaultSdlBase = 0x30000000;

constexpr uint64_t kUartSize = 0x100;
constexpr uint64_t kTimerSize = 0x100;

constexpr uint32_t kDefaultWidth = 640;
constexpr uint32_t kDefaultHeight = 480;

struct EmulatorConfig {
    std::string romPath;
    std::string configPath = "emulator.conf";
    std::string windowTitle = "Emulator";
    uint64_t romBase = kDefaultRomBase;
    uint64_t ramBase = kDefaultRamBase;
    uint64_t ramSize = kDefaultRamSize;
    uint64_t uartBase = kDefaultUartBase;
    uint64_t timerBase = kDefaultTimerBase;
    uint64_t sdlBase = kDefaultSdlBase;
    uint32_t width = kDefaultWidth;
    uint32_t height = kDefaultHeight;
    uint32_t cpuFrequency = 1000000;
    bool debug = false;
    bool showHelp = false;

    bool iTrace = false;
    bool mTrace = false;
    bool bpTrace = false;
    bool headless = false;
    std::string logLevel = "info";
    std::string logFilename = "";
};

bool loadConfigFile(const std::string& path, bool required, EmulatorConfig* config,
    std::string* error);

void printUsage(const char* exe);
bool findConfigPath(int argc, char** argv, EmulatorConfig* config, bool* required,
    std::string* error);
bool parseArgs(int argc, char** argv, EmulatorConfig* config, std::string* error);

bool validateMappings(const std::vector<MemoryRegion>& mappings, std::string* error);

class ICpuExecutor;
class TimerDevice;
class SdlDisplayDevice;
class MemoryBus;
class UartDevice;

int RunEmulator(int argc, char** argv);

#endif
