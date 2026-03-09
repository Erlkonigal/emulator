#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "emulator/generated/hardware_config.h"
#include "emulator/utils/singleton.h"

class RuntimeConfig : public Singleton<RuntimeConfig> {
public:
    std::string romPath;
    std::string configPath = "emulator.conf";
    bool debug = false;
    uint16_t debugPort = kDefaultDebugPort;
    uint64_t ramSize = kRamSize;
    
    std::vector<std::string> traceOn;
    std::string traceFile;
    std::string logLevel = "info";
    std::string logFile;
    bool showHelp = false;

    bool loadFromFile(const std::string &path, std::string *error);
    bool parseArgs(int argc, char **argv, std::string *error);
    bool findConfigPath(int argc, char **argv, bool *required, std::string *error);
    static void printUsage(const char *exe);
    void reset();

private:
    RuntimeConfig() = default;
    friend class Singleton<RuntimeConfig>;
};