#include "test_helpers.h"

#include <vector>

#include "emulator/cpu/cpu.h"
#include "toy_cpu_executor.h"

namespace testutil {

std::filesystem::path RomDir() {
    return std::filesystem::path("test") / "build" / "rom";
}

std::filesystem::path MakeRomPath(const std::string& name) {
    return RomDir() / (name + ".bin");
}

int RunEmuWithRom(const std::filesystem::path& romPath, bool debug, std::string* error) {
    std::vector<std::string> args;
    args.push_back("emulator_test");
    args.push_back("--rom");
    args.push_back(romPath.string());
    args.push_back("--width");
    args.push_back("16");
    args.push_back("--height");
    args.push_back("16");
    args.push_back("--ram-size");
    args.push_back("65536");
    if (debug) {
        args.push_back("--debug");
    }

    std::vector<char*> argv;
    argv.reserve(args.size());
    for (auto& s : args) {
        argv.push_back(s.data());
    }

    int rc = RunEmulator(static_cast<int>(argv.size()), argv.data());
    if (rc != 0 && error != nullptr) {
        *error = "RunEmulator returned non-zero";
    }
    return rc;
}

bool LastErrorIs(CpuErrorType t) {
    ToyCpuExecutor* cpu = GetLastToyCpu();
    if (cpu == nullptr) {
        return false;
    }
    return cpu->GetLastError().Type == t;
}

} // namespace testutil
