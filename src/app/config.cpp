#include "emulator/app.h"

#include <fstream>
#include <limits>
#include <string>

namespace {
bool ApplyConfigValue(EmulatorConfig* config, const std::string& key, const std::string& value,
    std::string* error) {
    if (config == nullptr) {
        return false;
    }
    if (key == "rom") {
        config->RomPath = value;
        return true;
    }
    if (key == "debug") {
        bool flag = false;
        if (!ParseBool(value, &flag)) {
            if (error != nullptr) {
                *error = "Invalid debug value: " + value;
            }
            return false;
        }
        config->Debug = flag;
        return true;
    }
    if (key == "width") {
        uint64_t parsed = 0;
        if (!ParseU64(value, &parsed) || parsed > std::numeric_limits<uint32_t>::max()) {
            if (error != nullptr) {
                *error = "Invalid width value: " + value;
            }
            return false;
        }
        config->Width = static_cast<uint32_t>(parsed);
        return true;
    }
    if (key == "height") {
        uint64_t parsed = 0;
        if (!ParseU64(value, &parsed) || parsed > std::numeric_limits<uint32_t>::max()) {
            if (error != nullptr) {
                *error = "Invalid height value: " + value;
            }
            return false;
        }
        config->Height = static_cast<uint32_t>(parsed);
        return true;
    }
    if (key == "ram_base") {
        uint64_t parsed = 0;
        if (!ParseU64(value, &parsed)) {
            if (error != nullptr) {
                *error = "Invalid ram_base value: " + value;
            }
            return false;
        }
        config->RamBase = parsed;
        return true;
    }
    if (key == "ram_size") {
        uint64_t parsed = 0;
        if (!ParseU64(value, &parsed)) {
            if (error != nullptr) {
                *error = "Invalid ram_size value: " + value;
            }
            return false;
        }
        config->RamSize = parsed;
        return true;
    }
    if (key == "uart_base") {
        uint64_t parsed = 0;
        if (!ParseU64(value, &parsed)) {
            if (error != nullptr) {
                *error = "Invalid uart_base value: " + value;
            }
            return false;
        }
        config->UartBase = parsed;
        return true;
    }
    if (key == "timer_base") {
        uint64_t parsed = 0;
        if (!ParseU64(value, &parsed)) {
            if (error != nullptr) {
                *error = "Invalid timer_base value: " + value;
            }
            return false;
        }
        config->TimerBase = parsed;
        return true;
    }
    if (key == "sdl_base") {
        uint64_t parsed = 0;
        if (!ParseU64(value, &parsed)) {
            if (error != nullptr) {
                *error = "Invalid sdl_base value: " + value;
            }
            return false;
        }
        config->SdlBase = parsed;
        return true;
    }
    if (key == "title") {
        config->WindowTitle = value;
        return true;
    }
    if (error != nullptr) {
        *error = "Unknown config key: " + key;
    }
    return false;
}
} // namespace

bool LoadConfigFile(const std::string& path, bool required, EmulatorConfig* config,
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
        StripInlineComment(&line);
        TrimInPlace(&line);
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
        TrimInPlace(&key);
        TrimInPlace(&value);
        if (!value.empty()) {
            if ((value.front() == '"' && value.back() == '"') ||
                (value.front() == '\'' && value.back() == '\'')) {
                if (value.size() >= 2) {
                    value = value.substr(1, value.size() - 2);
                }
            }
        }
        key = ToLower(key);
        if (!ApplyConfigValue(config, key, value, error)) {
            return false;
        }
    }
    return true;
}
