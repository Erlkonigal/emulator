#include "emulator/runtime_config.h"
#include "emulator/utils/utils.h"

#include <cstdio>
#include <fstream>
#include <limits>
#include <string>

namespace {
bool applyConfigValue(RuntimeConfig *config, const std::string &key,
                      const std::string &value, std::string *error) {
    if (config == nullptr) {
        return false;
    }
    if (key == "rom") {
        config->romPath = value;
        return true;
    }
    if (key == "debug") {
        bool flag = false;
        if (!parseBool(value, &flag)) {
            if (error != nullptr) {
                *error = "Invalid debug value: " + value;
            }
            return false;
        }
        config->debug = flag;
        return true;
    }
    if (key == "debug_port") {
        uint64_t parsed = 0;
        if (!parseU64(value, &parsed) ||
            parsed > std::numeric_limits<uint16_t>::max()) {
            if (error != nullptr) {
                *error = "Invalid debug_port value: " + value;
            }
            return false;
        }
        config->debugPort = static_cast<uint16_t>(parsed);
        return true;
    }
    if (key == "trace_on") {
        config->traceOn.push_back(value);
        return true;
    }
    if (key == "log_level") {
        config->logLevel = value;
        return true;
    }
    if (key == "trace_file") {
        config->traceFile = value;
        return true;
    }
    if (key == "log_file") {
        config->logFile = value;
        return true;
    }
    
    if (error != nullptr) {
        *error = "Unknown config key: " + key;
    }
    return false;
}
}

void RuntimeConfig::reset() {
    romPath.clear();
    configPath = "emulator.conf";
    debug = false;
    debugPort = kDefaultDebugPort;
    ramSize = kRamSize;
    traceOn.clear();
    traceFile.clear();
    logLevel = "info";
    logFile.clear();
    showHelp = false;
}

bool RuntimeConfig::loadFromFile(const std::string &path, std::string *error) {
    std::ifstream input(path);
    if (!input.is_open()) {
        return false;
    }
    std::string line;
    size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        stripInlineComment(&line);
        trimInPlace(&line);
        if (line.empty()) {
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos) {
            if (error != nullptr) {
                *error = "Invalid config line " + std::to_string(lineNumber);
            }
            return false;
        }
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        trimInPlace(&key);
        trimInPlace(&value);
        if (!value.empty()) {
            if ((value.front() == '"' && value.back() == '"') ||
                (value.front() == '\'' && value.back() == '\'')) {
                if (value.size() >= 2) {
                    value = value.substr(1, value.size() - 2);
                }
            }
        }
        key = toLower(key);
        if (!applyConfigValue(this, key, value, error)) {
            return false;
        }
    }
    return true;
}

void RuntimeConfig::printUsage(const char *exe) {
    const char *name = (exe != nullptr) ? exe : "emulator";
    std::fprintf(
        stdout,
        "Usage: %s --rom <path> [options]\n"
        "\n"
        "Options:\n"
        "  --config <file>       Load config file (default: emulator.conf)\n"
        "  --debug               Start in debugger mode (pause on start, debug server on port 1234)\n"
        "  --trace-on <name>     Enable trace by name (can be used multiple times)\n"
        "  --trace-file <file>   Output trace to file\n"
        "  --log-level <lvl>     Set log level (debug, info, warn, error)\n"
        "  --log-file <file>     Output log to file\n"
        "  --help, -h            Show this help\n",
        name);
}

bool RuntimeConfig::findConfigPath(int argc, char **argv, bool *required,
                                   std::string *error) {
    if (required == nullptr) {
        return false;
    }
    *required = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            showHelp = true;
            continue;
        }
        if (arg == "--config") {
            std::string value;
            if (!requireArgValue(argc, argv, &i, "--config", &value, error)) {
                return false;
            }
            configPath = value;
            *required = true;
            continue;
        }
    }
    return true;
}

bool RuntimeConfig::parseArgs(int argc, char **argv, std::string *error) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            showHelp = true;
            continue;
        }
        if (arg == "--config") {
            std::string value;
            if (!requireArgValue(argc, argv, &i, "--config", &value, error)) {
                return false;
            }
            configPath = value;
            continue;
        }
        if (arg == "--rom") {
            std::string value;
            if (!requireArgValue(argc, argv, &i, "--rom", &value, error)) {
                return false;
            }
            romPath = value;
            continue;
        }
if (arg == "--debug") {
            debug = true;
            continue;
        }
        if (arg == "--ram-size") {
            std::string value;
            if (!requireArgValue(argc, argv, &i, "--ram-size", &value, error)) {
                return false;
            }
            if (!parseU64(value, &ramSize)) {
                if (error != nullptr) {
                    *error = "Invalid ram-size value: " + value;
                }
                return false;
            }
            continue;
        }
        if (arg == "--trace-on") {
            std::string value;
            if (!requireArgValue(argc, argv, &i, "--trace-on", &value, error)) {
                return false;
            }
            traceOn.push_back(value);
            continue;
        }
        if (arg == "--trace-file") {
            std::string value;
            if (!requireArgValue(argc, argv, &i, "--trace-file", &value, error)) {
                return false;
            }
            traceFile = value;
            continue;
        }
        if (arg == "--log-level") {
            std::string value;
            if (!requireArgValue(argc, argv, &i, "--log-level", &value, error)) {
                return false;
            }
            logLevel = value;
            continue;
        }
        if (arg == "--log-file") {
            std::string value;
            if (!requireArgValue(argc, argv, &i, "--log-file", &value, error)) {
                return false;
            }
            logFile = value;
            continue;
        }
        if (!arg.empty() && arg[0] == '-') {
            if (error != nullptr) {
                *error = "Unknown option: " + std::string(arg);
            }
            return false;
        }
        if (romPath.empty()) {
            romPath = std::string(arg);
            continue;
        }
        if (error != nullptr) {
            *error = "Unexpected argument: " + std::string(arg);
        }
        return false;
    }
    return true;
}