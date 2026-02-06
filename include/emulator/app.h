#ifndef EMULATOR_APP_APP_H
#define EMULATOR_APP_APP_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "emulator/bus.h"
#include "emulator/cpu.h"

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
    bool Debug = false;
    bool ShowHelp = false;
};

bool LoadConfigFile(const std::string& path, bool required, EmulatorConfig* config,
    std::string* error);

void PrintUsage(const char* exe);
bool FindConfigPath(int argc, char** argv, EmulatorConfig* config, bool* required,
    std::string* error);
bool ParseArgs(int argc, char** argv, EmulatorConfig* config, std::string* error);

bool GetFileSize(const std::string& path, uint64_t* size);
bool ComputeFramebufferSize(uint32_t width, uint32_t height, uint64_t* size);
bool ValidateMappings(const std::vector<MemoryRegion>& mappings, std::string* error);

void TrimInPlace(std::string* text);
void StripInlineComment(std::string* line);
std::string ToLower(const std::string& text);
bool ParseU64(const std::string& text, uint64_t* value);
bool ParseBool(const std::string& text, bool* value);
bool RequireArgValue(int argc, char** argv, int* index, const char* option, std::string* out,
    std::string* error);
bool ParseU32Arg(const char* option, const std::string& text, uint32_t* out, std::string* error);
bool ParseU64Arg(const char* option, const std::string& text, uint64_t* out, std::string* error);

class ICpuExecutor;
class TimerDevice;
class SdlDisplayDevice;
class MemoryBus;
class UartDevice;

struct CpuControl {
    std::mutex Mutex;
    std::condition_variable Cv;
};

struct EmulatorRunState {
    std::atomic<CpuState> State{CpuState::Start};
    std::atomic<uint32_t> StepsPending{0};
};


void PumpTerminalInput(UartDevice* uart);
void RunCpuThread(ICpuExecutor* cpu, TimerDevice* timer, EmulatorRunState* state,
    CpuControl* control);
void RunSdlThread(SdlDisplayDevice* sdl, EmulatorRunState* state, CpuControl* control);
void RunDebugger(ICpuExecutor* cpu, MemoryBus* bus, UartDevice* uart, EmulatorRunState* state,
    CpuControl* control);

int RunEmulator(int argc, char** argv);

#endif
