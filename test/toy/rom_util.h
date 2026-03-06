#ifndef TEST_ROM_UTIL_H
#define TEST_ROM_UTIL_H

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace rom {

inline bool WriteRomU32LE(const std::filesystem::path& path, const std::vector<uint32_t>& insts,
    std::string* error) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        if (error != nullptr) {
            *error = "create_directories failed";
        }
        return false;
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        if (error != nullptr) {
            *error = "open failed";
        }
        return false;
    }
    for (uint32_t w : insts) {
        uint8_t b[4];
        b[0] = static_cast<uint8_t>(w & 0xff);
        b[1] = static_cast<uint8_t>((w >> 8) & 0xff);
        b[2] = static_cast<uint8_t>((w >> 16) & 0xff);
        b[3] = static_cast<uint8_t>((w >> 24) & 0xff);
        out.write(reinterpret_cast<const char*>(b), 4);
        if (!out.good()) {
            if (error != nullptr) {
                *error = "write failed";
            }
            return false;
        }
    }
    out.flush();
    return out.good();
}

} // namespace rom

#endif
