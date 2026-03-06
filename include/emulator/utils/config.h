#pragma once

#include <cstdint>
#include <string>
#include <vector>

constexpr const char *kRomDeviceName = "ROM";
constexpr const char *kRamDeviceName = "RAM";
constexpr const char *kUartDeviceName = "UART";
constexpr const char *kDisplayDeviceName = "Display";

constexpr uint64_t kRomBase = 0x00000000;
constexpr uint64_t kRamBase = 0x80000000;
constexpr uint64_t kUartBase = 0x20000000;
constexpr uint64_t kDisplayBase = 0x30000000;

constexpr uint64_t kRamSize = 256ull * 1024 * 1024;
constexpr uint64_t kUartSize = 0x100;
constexpr uint64_t kDisplayControlRegionSize = 0x1000;

constexpr uint32_t kDisplayWidth = 640;
constexpr uint32_t kDisplayHeight = 480;

// mem
constexpr uint8_t kMemInitByte = 0xcd;

// display
constexpr bool kDisplayHeadless = false;
constexpr uint64_t kDisplayFrameBufferOffset = kDisplayControlRegionSize;
constexpr const char *kDisplayWindowTitle = "Emulator";

// queue
constexpr uint32_t kDefaultCommitQueueSize = 1024;

// cpu
constexpr uint32_t kMaxNumRegisters = 32;
constexpr uint32_t kMaxNumCsr = 64;
constexpr uint32_t kMaxNumCommitsPerCycle = 8;

// commit consumer
constexpr uint32_t kMaxNumCommitsPerConsumption = 256;

// align
constexpr size_t kPadding = 64;

struct EmulatorConfig {
  std::string romPath;
  std::string configPath = "emulator.conf";
  std::string windowTitle = "Emulator";
  uint64_t romBase = kRomBase;
  uint64_t ramBase = kRamBase;
  uint64_t ramSize = kRamSize;
  uint64_t uartBase = kUartBase;
  uint64_t displayBase = kDisplayBase;
  uint32_t width = kDisplayWidth;
  uint32_t height = kDisplayHeight;
  uint32_t cpuFrequency = 1000000;
  bool debug = false;
  bool showHelp = false;

  bool iTrace = false;
  bool mTrace = false;
  bool bpTrace = false;
  bool headless = false;
  std::string logLevel = "info";
  std::string logFilename = "";

  static bool loadConfigFile(const std::string &path, bool required,
                             EmulatorConfig *config, std::string *error);
  static bool findConfigPath(int argc, char **argv, EmulatorConfig *config,
                             bool *required, std::string *error);
  static void printUsage(const char *exe);
  static bool parseArgs(int argc, char **argv, EmulatorConfig *config,
                        std::string *error);
};
