#include "emulator/app/app.h"
#include "emulator/app/utils.h"

#include <fstream>
#include <limits>
#include <string>

namespace {
bool applyConfigValue(EmulatorConfig* config, const std::string& key, const std::string& value,
    std::string* error) {
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
    if (key == "itrace") {
        bool flag = false;
        if (!parseBool(value, &flag)) {
            if (error != nullptr) *error = "Invalid itrace value: " + value;
            return false;
        }
        config->iTrace = flag;
        return true;
    }
    if (key == "mtrace") {
        bool flag = false;
        if (!parseBool(value, &flag)) {
            if (error != nullptr) *error = "Invalid mtrace value: " + value;
            return false;
        }
        config->mTrace = flag;
        return true;
    }
    if (key == "bptrace") {
        bool flag = false;
        if (!parseBool(value, &flag)) {
            if (error != nullptr) *error = "Invalid bptrace value: " + value;
            return false;
        }
        config->bpTrace = flag;
        return true;
    }
    if (key == "log_level") {
        config->logLevel = value;
        return true;
    }
    if (key == "log_filename") {
        config->logFilename = value;
        return true;
    }
    if (key == "headless") {
        bool flag = false;
        if (!parseBool(value, &flag)) {
            if (error != nullptr) {
                *error = "Invalid headless value: " + value;
            }
            return false;
        }
        config->headless = flag;
        return true;
    }
    if (key == "width") {
        uint64_t parsed = 0;
        if (!parseU64(value, &parsed) || parsed > std::numeric_limits<uint32_t>::max()) {
            if (error != nullptr) {
                *error = "Invalid width value: " + value;
            }
            return false;
        }
        config->width = static_cast<uint32_t>(parsed);
        return true;
    }
    if (key == "height") {
        uint64_t parsed = 0;
        if (!parseU64(value, &parsed) || parsed > std::numeric_limits<uint32_t>::max()) {
            if (error != nullptr) {
                *error = "Invalid height value: " + value;
            }
            return false;
        }
        config->height = static_cast<uint32_t>(parsed);
        return true;
    }
    if (key == "ram_base") {
        uint64_t parsed = 0;
        if (!parseU64(value, &parsed)) {
            if (error != nullptr) {
                *error = "Invalid ram_base value: " + value;
            }
            return false;
        }
        config->ramBase = parsed;
        return true;
    }
    if (key == "ram_size") {
        uint64_t parsed = 0;
        if (!parseU64(value, &parsed)) {
            if (error != nullptr) {
                *error = "Invalid ram_size value: " + value;
            }
            return false;
        }
        config->ramSize = parsed;
        return true;
    }
    if (key == "uart_base") {
        uint64_t parsed = 0;
        if (!parseU64(value, &parsed)) {
            if (error != nullptr) {
                *error = "Invalid uart_base value: " + value;
            }
            return false;
        }
        config->uartBase = parsed;
        return true;
    }
    if (key == "timer_base") {
        uint64_t parsed = 0;
        if (!parseU64(value, &parsed)) {
            if (error != nullptr) {
                *error = "Invalid timer_base value: " + value;
            }
            return false;
        }
        config->timerBase = parsed;
        return true;
    }
    if (key == "sdl_base") {
        uint64_t parsed = 0;
        if (!parseU64(value, &parsed)) {
            if (error != nullptr) {
                *error = "Invalid sdl_base value: " + value;
            }
            return false;
        }
        config->sdlBase = parsed;
        return true;
    }
    if (key == "title") {
        config->windowTitle = value;
        return true;
    }
    if (key == "cpu_frequency") {
        uint64_t parsed = 0;
        if (!parseU64(value, &parsed) || parsed > std::numeric_limits<uint32_t>::max()) {
            if (error != nullptr) {
                *error = "Invalid cpu_frequency value: " + value;
            }
            return false;
        }
        config->cpuFrequency = static_cast<uint32_t>(parsed);
        return true;
    }
    if (error != nullptr) {
        *error = "Unknown config key: " + key;
    }
    return false;
}
} // namespace

bool loadConfigFile(const std::string& path, bool required, EmulatorConfig* config,
    std::string* error) {
    std::ifstream input(path);
    if (!input.is_open()) {
        if (required) {
            if (error != nullptr) {
                *error = "Failed to open config file: " + path;
            }
            return false;
        }
        return true;
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
        if (!applyConfigValue(config, key, value, error)) {
            return false;
        }
    }
    return true;
}
