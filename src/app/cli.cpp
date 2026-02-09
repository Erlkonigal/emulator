#include "emulator/app/app.h"
#include "emulator/app/utils.h"

#include <cstdio>
#include <limits>
#include <string>

void PrintUsage(const char* exe) {
    const char* name = (exe != nullptr) ? exe : "emulator";
    std::fprintf(stdout,
        "Usage: %s --rom <path> [options]\n"
        "\n"
        "Options:\n"
        "  --config <file>   Load config file (default: emulator.conf)\n"
        "  --debug           Start in debugger mode\n"
        "  --width <pixels>  SDL width (default: 640)\n"
        "  --height <pixels> SDL height (default: 480)\n"
        "  --sdl-base <addr> SDL base address (default: 0x30000000)\n"
        "  --ram-base <addr> RAM base address (default: 0x80000000)\n"
        "  --ram-size <bytes> RAM size (default: 268435456)\n"
        "  --uart-base <addr> UART base address (default: 0x20000000)\n"
        "  --timer-base <addr> TIMER base address (default: 0x20001000)\n"
        "  --title <string> Window title (default: Emulator)\n"
        "  --itrace          Enable Instruction Trace\n"
        "  --mtrace          Enable Memory Trace\n"
        "  --bptrace         Enable Branch Prediction Trace\n"
        "  --log-level <lvl>     Set log level (trace, debug, info, warn, error)\n"
        "  --log-filename <path> Set log file path (device->name.out, other->name.err)\n"
        "  --enable-log          Enable separate logging (splits device/other output)\n"
        "  --help, -h            Show this help\n",
        name);
}

bool FindConfigPath(int argc, char** argv, EmulatorConfig* config, bool* required,
    std::string* error) {
    if (config == nullptr || required == nullptr) {
        return false;
    }
    *required = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            config->ShowHelp = true;
            continue;
        }
        if (arg == "--config") {
            std::string value;
            if (!RequireArgValue(argc, argv, &i, "--config", &value, error)) {
                return false;
            }
            config->ConfigPath = value;
            *required = true;
            continue;
        }
    }
    return true;
}

bool ParseArgs(int argc, char** argv, EmulatorConfig* config, std::string* error) {
    if (config == nullptr) {
        return false;
    }
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            config->ShowHelp = true;
            continue;
        }
        if (arg == "--config") {
            std::string value;
            if (!RequireArgValue(argc, argv, &i, "--config", &value, error)) {
                return false;
            }
            config->ConfigPath = value;
            continue;
        }
        if (arg == "--rom") {
            std::string value;
            if (!RequireArgValue(argc, argv, &i, "--rom", &value, error)) {
                return false;
            }
            config->RomPath = value;
            continue;
        }
        if (arg == "--debug") {
            config->Debug = true;
            continue;
        }
        if (arg == "--width") {
            std::string value;
            if (!RequireArgValue(argc, argv, &i, "--width", &value, error)) {
                return false;
            }
            if (!ParseU32Arg("width", value, &config->Width, error)) {
                return false;
            }
            continue;
        }
        if (arg == "--height") {
            std::string value;
            if (!RequireArgValue(argc, argv, &i, "--height", &value, error)) {
                return false;
            }
            if (!ParseU32Arg("height", value, &config->Height, error)) {
                return false;
            }
            continue;
        }
        if (arg == "--sdl-base") {
            std::string value;
            if (!RequireArgValue(argc, argv, &i, "--sdl-base", &value, error)) {
                return false;
            }
            if (!ParseU64Arg("sdl-base", value, &config->SdlBase, error)) {
                return false;
            }
            continue;
        }
        if (arg == "--ram-base") {
            std::string value;
            if (!RequireArgValue(argc, argv, &i, "--ram-base", &value, error)) {
                return false;
            }
            if (!ParseU64Arg("ram-base", value, &config->RamBase, error)) {
                return false;
            }
            continue;
        }
        if (arg == "--ram-size") {
            std::string value;
            if (!RequireArgValue(argc, argv, &i, "--ram-size", &value, error)) {
                return false;
            }
            if (!ParseU64Arg("ram-size", value, &config->RamSize, error)) {
                return false;
            }
            continue;
        }
        if (arg == "--uart-base") {
            std::string value;
            if (!RequireArgValue(argc, argv, &i, "--uart-base", &value, error)) {
                return false;
            }
            if (!ParseU64Arg("uart-base", value, &config->UartBase, error)) {
                return false;
            }
            continue;
        }
        if (arg == "--timer-base") {
            std::string value;
            if (!RequireArgValue(argc, argv, &i, "--timer-base", &value, error)) {
                return false;
            }
            if (!ParseU64Arg("timer-base", value, &config->TimerBase, error)) {
                return false;
            }
            continue;
        }
        if (arg == "--title") {
            std::string value;
            if (!RequireArgValue(argc, argv, &i, "--title", &value, error)) {
                return false;
            }
            config->WindowTitle = value;
            continue;
        }
        if (arg == "--itrace") {
            config->ITrace = true;
            continue;
        }
        if (arg == "--mtrace") {
            config->MTrace = true;
            continue;
        }
        if (arg == "--bptrace") {
            config->BPTrace = true;
            continue;
        }
        if (arg == "--log-level") {
            std::string value;
            if (!RequireArgValue(argc, argv, &i, "--log-level", &value, error)) {
                return false;
            }
            config->LogLevel = value;
            continue;
        }
        if (arg == "--log-filename") {
            std::string value;
            if (!RequireArgValue(argc, argv, &i, "--log-filename", &value, error)) {
                return false;
            }
            config->LogFilename = value;
            continue;
        }
        if (arg == "--enable-log") {
            config->EnableLog = true;
            continue;
        }
        if (!arg.empty() && arg[0] == '-') {
            if (error != nullptr) {
                *error = "Unknown option: " + std::string(arg);
            }
            return false;
        }
        if (config->RomPath.empty()) {
            config->RomPath = std::string(arg);
            continue;
        }
        if (error != nullptr) {
            *error = "Unexpected argument: " + std::string(arg);
        }
        return false;
    }
    return true;
}
