#ifndef TEST_TEST_HELPERS_H
#define TEST_TEST_HELPERS_H

#include <filesystem>
#include <string>

#include "emulator/app/app.h"

namespace testutil {

std::filesystem::path RomDir();
std::filesystem::path MakeRomPath(const std::string& name);

int RunEmuWithRom(const std::filesystem::path& romPath, bool debug, std::string* error);
bool LastErrorIs(CpuErrorType t);

} // namespace testutil

#endif
