#include "emulator/app/app.h"
#include "emulator/app/utils.h"

#include <cstdio>
#include <limits>
#include <string>

void printUsage(const char* exe) {
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
        "  --ram-size <bytes> RAM size (default: 0x10000000)\n"
        "  --uart-base <addr> UART base address (default: 0x20000000)\n"
        "  --timer-base <addr> TIMER base address (default: 0x20001000)\n"
        "  --title <string> Window title (default: Emulator)\n"
        "  --itrace          Enable Instruction Trace\n"
        "  --mtrace          Enable Memory Trace\n"
        "  --bptrace         Enable Branch Prediction Trace\n"
        "  --log-level <lvl>     Set log level (trace, debug, info, warn, error)\n"
        "  --log-filename <path> Set log file path (device->name.out, other->name.err)\n"
        "  --headless            Run without SDL window (headless mode)\n"
        "  --help, -h            Show this help\n",
        name);
}

bool findConfigPath(int argc, char** argv, EmulatorConfig* config, bool* required,
    std::string* error) {
    if (config == nullptr || required == nullptr) {
        return false;
    }
    *required = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            config->showHelp = true;
            continue;
        }
        if (arg == "--config") {
            std::string value;
            if (!requireArgValue(argc, argv, &i, "--config", &value, error)) {
                return false;
            }
            config->configPath = value;
            *required = true;
            continue;
        }
    }
    return true;
}

bool parseArgs(int argc, char** argv, EmulatorConfig* config, std::string* error) {
    if (config == nullptr) {
        return false;
    }
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            config->showHelp = true;
            continue;
        }
        if (arg == "--config") {
            std::string value;
            if (!requireArgValue(argc, argv, &i, "--config", &value, error)) {
                return false;
            }
            config->configPath = value;
            continue;
        }
        if (arg == "--rom") {
            std::string value;
            if (!requireArgValue(argc, argv, &i, "--rom", &value, error)) {
                return false;
            }
            config->romPath = value;
            continue;
        }
        if (arg == "--debug") {
            config->debug = true;
            continue;
        }
        if (arg == "--width") {
            std::string value;
            if (!requireArgValue(argc, argv, &i, "--width", &value, error)) {
                return false;
            }
            if (!parseU32Arg("width", value, &config->width, error)) {
                return false;
            }
            continue;
        }
        if (arg == "--height") {
            std::string value;
            if (!requireArgValue(argc, argv, &i, "--height", &value, error)) {
                return false;
            }
            if (!parseU32Arg("height", value, &config->height, error)) {
                return false;
            }
            continue;
        }
        if (arg == "--sdl-base") {
            std::string value;
            if (!requireArgValue(argc, argv, &i, "--sdl-base", &value, error)) {
                return false;
            }
            if (!parseU64Arg("sdl-base", value, &config->sdlBase, error)) {
                return false;
            }
            continue;
        }
        if (arg == "--ram-base") {
            std::string value;
            if (!requireArgValue(argc, argv, &i, "--ram-base", &value, error)) {
                return false;
            }
            if (!parseU64Arg("ram-base", value, &config->ramBase, error)) {
                return false;
            }
            continue;
        }
        if (arg == "--ram-size") {
            std::string value;
            if (!requireArgValue(argc, argv, &i, "--ram-size", &value, error)) {
                return false;
            }
            if (!parseU64Arg("ram-size", value, &config->ramSize, error)) {
                return false;
            }
            continue;
        }
        if (arg == "--uart-base") {
            std::string value;
            if (!requireArgValue(argc, argv, &i, "--uart-base", &value, error)) {
                return false;
            }
            if (!parseU64Arg("uart-base", value, &config->uartBase, error)) {
                return false;
            }
            continue;
        }
        if (arg == "--timer-base") {
            std::string value;
            if (!requireArgValue(argc, argv, &i, "--timer-base", &value, error)) {
                return false;
            }
            if (!parseU64Arg("timer-base", value, &config->timerBase, error)) {
                return false;
            }
            continue;
        }
        if (arg == "--title") {
            std::string value;
            if (!requireArgValue(argc, argv, &i, "--title", &value, error)) {
                return false;
            }
            config->windowTitle = value;
            continue;
        }
        if (arg == "--itrace") {
            config->iTrace = true;
            continue;
        }
        if (arg == "--mtrace") {
            config->mTrace = true;
            continue;
        }
        if (arg == "--bptrace") {
            config->bpTrace = true;
            continue;
        }
        if (arg == "--log-level") {
            std::string value;
            if (!requireArgValue(argc, argv, &i, "--log-level", &value, error)) {
                return false;
            }
            config->logLevel = value;
            continue;
        }
        if (arg == "--log-filename") {
            std::string value;
            if (!requireArgValue(argc, argv, &i, "--log-filename", &value, error)) {
                return false;
            }
            config->logFilename = value;
            continue;
        }
        if (arg == "--headless") {
            config->headless = true;
            continue;
        }
        if (!arg.empty() && arg[0] == '-') {
            if (error != nullptr) {
                *error = "Unknown option: " + std::string(arg);
            }
            return false;
        }
        if (config->romPath.empty()) {
            config->romPath = std::string(arg);
            continue;
        }
        if (error != nullptr) {
            *error = "Unexpected argument: " + std::string(arg);
        }
        return false;
    }
    return true;
}
