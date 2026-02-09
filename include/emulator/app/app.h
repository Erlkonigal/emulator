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
    std::string RomPath;
    std::string ConfigPath = "emulator.conf";
    std::string WindowTitle = "Emulator";
    uint64_t RomBase = kDefaultRomBase;
    uint64_t RamBase = kDefaultRamBase;
    uint64_t RamSize = kDefaultRamSize;
    uint64_t UartBase = kDefaultUartBase;
    uint64_t TimerBase = kDefaultTimerBase;
    uint64_t SdlBase = kDefaultSdlBase;
    uint32_t Width = kDefaultWidth;
    uint32_t Height = kDefaultHeight;
    uint32_t CpuFrequency = 1000000;
    bool Debug = false;
    bool ShowHelp = false;

    // Trace & Logging
    bool ITrace = false;
    bool MTrace = false;
    bool BPTrace = false;
    std::string LogLevel = "info";
    std::string LogFilename = "";
    bool EnableLog = false;
};

bool LoadConfigFile(const std::string& path, bool required, EmulatorConfig* config,
    std::string* error);

void PrintUsage(const char* exe);
bool FindConfigPath(int argc, char** argv, EmulatorConfig* config, bool* required,
    std::string* error);
bool ParseArgs(int argc, char** argv, EmulatorConfig* config, std::string* error);

bool ValidateMappings(const std::vector<MemoryRegion>& mappings, std::string* error);

class ICpuExecutor;
class TimerDevice;
class SdlDisplayDevice;
class MemoryBus;
class UartDevice;

int RunEmulator(int argc, char** argv);

#endif
