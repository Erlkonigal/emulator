#ifndef TEST_TEST_HELPERS_H
#define TEST_TEST_HELPERS_H

#include <filesystem>
#include <string>

#include "emulator/app.h"
#include "emulator/cpu/cpu.h"

namespace testutil {

using emulator::CpuErrorType;

std::filesystem::path RomDir();
std::filesystem::path MakeRomPath(const std::string &name);

int RunEmuWithRom(const std::filesystem::path &romPath, bool debug,
                  std::string *error);
bool CheckLastError(CpuErrorType t);

} // namespace testutil

#endif
