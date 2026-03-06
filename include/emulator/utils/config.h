#pragma once

#include <cstdint>
#include <string>

constexpr const char *kRomDeviceName = "ROM";
constexpr const char *kRamDeviceName = "RAM";

constexpr uint64_t kRomBase = 0x00000000;
constexpr uint64_t kRamBase = 0x80000000;

constexpr uint64_t kRamSize = 256ull * 1024 * 1024;

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
  uint64_t romBase = kRomBase;
  uint64_t ramBase = kRamBase;
  uint64_t ramSize = kRamSize;
  uint32_t cpuFrequency = 1000000;
  bool debug = false;
  bool showHelp = false;

  bool iTrace = false;
  bool mTrace = false;
  bool bpTrace = false;
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