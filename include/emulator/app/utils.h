#ifndef EMULATOR_APP_UTILS_H
#define EMULATOR_APP_UTILS_H

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace {
inline bool isSpaceChar(char ch) {
    return std::isspace(static_cast<unsigned char>(ch)) != 0;
}

inline char toLowerChar(char ch) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
}

inline bool isEmpty(const std::string& text) {
    return text.empty();
}

inline bool isHexDigitChar(char ch) {
    return std::isxdigit(static_cast<unsigned char>(ch)) != 0;
}
} // namespace

inline std::string toLower(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char ch : text) {
        out.push_back(toLowerChar(ch));
    }
    return out;
}

inline void trimInPlace(std::string* text) {
    if (text == nullptr || isEmpty(*text)) {
        return;
    }
    size_t start = 0;
    while (start < text->size() && isSpaceChar((*text)[start])) {
        ++start;
    }
    size_t end = text->size();
    while (end > start && isSpaceChar((*text)[end - 1])) {
        --end;
    }
    if (start == 0 && end == text->size()) {
        return;
    }
    *text = text->substr(start, end - start);
}

inline void stripInlineComment(std::string* line) {
    if (line == nullptr) {
        return;
    }
    size_t pos = line->find_first_of("#;");
    if (pos != std::string::npos) {
        line->erase(pos);
    }
}

inline bool parseBool(const std::string& text, bool* value) {
    if (value == nullptr) {
        return false;
    }
    std::string lowered = toLower(text);
    if (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on") {
        *value = true;
        return true;
    }
    if (lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off") {
        *value = false;
        return true;
    }
    return false;
}

inline bool parseU64(const std::string& text, uint64_t* value) {
    if (value == nullptr) {
        return false;
    }
    if (isEmpty(text)) {
        return false;
    }
    errno = 0;
    if (text.size() >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        uint64_t parsed = 0;
        for (size_t i = 2; i < text.size(); ++i) {
            char ch = text[i];
            if (!isHexDigitChar(ch)) {
                return false;
            }
            parsed <<= 4;
            if (ch >= '0' && ch <= '9') {
                parsed |= static_cast<uint64_t>(ch - '0');
            } else if (ch >= 'a' && ch <= 'f') {
                parsed |= static_cast<uint64_t>(ch - 'a' + 10);
            } else if (ch >= 'A' && ch <= 'F') {
                parsed |= static_cast<uint64_t>(ch - 'A' + 10);
            }
        }
        *value = parsed;
        return true;
    }
    uint64_t parsed = 0;
    for (char ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }
        parsed = parsed * 10 + static_cast<uint64_t>(ch - '0');
    }
    *value = parsed;
    return true;
}

inline bool getFileSize(const std::string& path, uint64_t* size) {
    if (size == nullptr) {
        return false;
    }
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }
    std::streamsize length = file.tellg();
    if (length < 0) {
        return false;
    }
    *size = static_cast<uint64_t>(length);
    return true;
}

inline bool computeFramebufferSize(uint32_t width, uint32_t height, uint64_t* size) {
    if (size == nullptr) {
        return false;
    }
    if (width == 0 || height == 0) {
        return false;
    }
    uint64_t pixelCount = static_cast<uint64_t>(width) * height;
    if (pixelCount / width != height) {
        return false;
    }
    if (pixelCount > std::numeric_limits<uint64_t>::max() / 4u) {
        return false;
    }
    *size = pixelCount * 4u;
    return true;
}

inline bool requireArgValue(int argc, char** argv, int* index, const char* option, std::string* out,
    std::string* error) {
    if (index == nullptr || out == nullptr) {
        return false;
    }
    if (*index + 1 >= argc) {
        if (error != nullptr) {
            *error = std::string(option) + " requires a value";
        }
        return false;
    }
    *out = argv[*index + 1];
    ++(*index);
    return true;
}

inline bool parseU32Arg(const char* option, const std::string& text, uint32_t* out, std::string* error) {
    uint64_t parsed = 0;
    if (!parseU64(text, &parsed) || parsed > std::numeric_limits<uint32_t>::max()) {
        if (error != nullptr) {
            *error = std::string("Invalid ") + option + " value";
        }
        return false;
    }
    *out = static_cast<uint32_t>(parsed);
    return true;
}

inline bool parseU64Arg(const char* option, const std::string& text, uint64_t* out, std::string* error) {
    uint64_t parsed = 0;
    if (!parseU64(text, &parsed)) {
        if (error != nullptr) {
            *error = std::string("Invalid ") + option + " value";
        }
        return false;
    }
    *out = parsed;
    return true;
}

#endif // EMULATOR_APP_UTILS_H
