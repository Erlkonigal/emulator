#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>


#include "emulator/app.h"
#include "emulator/cpu.h"

#include "rom_util.h"
#include "stdout_capture.h"
#include "test_framework.h"
#include "toy_cpu_executor.h"
#include "toy_isa.h"

namespace {

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

} // namespace

void RegisterIntegrationTests() {
    // Ensure test binary links all TEST() TU initializers.
}

TEST(integration_uart_ok) {
    std::vector<uint32_t> prog;
    toy::Emit(&prog, toy::Lui(1, 0x2000)); // r1 = 0x20000000

    // r2 = ch, store to [r1+0]
    toy::Emit(&prog, toy::Lui(2, 0x0));
    toy::Emit(&prog, toy::Ori(2, static_cast<uint16_t>('O')));
    toy::Emit(&prog, toy::Sw(2, 1, 0));
    toy::Emit(&prog, toy::Lui(2, 0x0));
    toy::Emit(&prog, toy::Ori(2, static_cast<uint16_t>('K')));
    toy::Emit(&prog, toy::Sw(2, 1, 0));
    toy::Emit(&prog, toy::Lui(2, 0x0));
    toy::Emit(&prog, toy::Ori(2, static_cast<uint16_t>('\n')));
    toy::Emit(&prog, toy::Sw(2, 1, 0));
    toy::Emit(&prog, toy::Halt());

    std::string err;
    auto romPath = MakeRomPath("uart_ok");
    ASSERT_TRUE(rom::WriteRomU32LE(romPath, prog, &err));

    testutil::StdoutCapture cap;
    ASSERT_TRUE(cap.Start(&err));
    int rc = RunEmuWithRom(romPath, false, &err);
    std::string out;
    ASSERT_TRUE(cap.Stop(&out, &err));

    ASSERT_EQ(rc, 0);
    EXPECT_TRUE(LastErrorIs(CpuErrorType::Halt));
    EXPECT_TRUE(out.find("OK\n") != std::string::npos);
}

TEST(integration_ram_rw) {
    std::vector<uint32_t> prog;
    toy::Emit(&prog, toy::Lui(1, 0x8000)); // r1 = 0x80000000

    toy::Emit(&prog, toy::Lui(2, 0x1122));
    toy::Emit(&prog, toy::Ori(2, 0x3344)); // r2 = 0x11223344
    toy::Emit(&prog, toy::Sw(2, 1, 0));
    toy::Emit(&prog, toy::Lw(3, 1, 0));
    toy::Emit(&prog, toy::Halt());

    std::string err;
    auto romPath = MakeRomPath("ram_rw");
    ASSERT_TRUE(rom::WriteRomU32LE(romPath, prog, &err));

    int rc = RunEmuWithRom(romPath, false, &err);
    ASSERT_EQ(rc, 0);

    ToyCpuExecutor* cpu = GetLastToyCpu();
    ASSERT_TRUE(cpu != nullptr);
    EXPECT_TRUE(LastErrorIs(CpuErrorType::Halt));
    EXPECT_EQ(static_cast<uint32_t>(cpu->GetRegister(3) & 0xffffffffu), 0x11223344u);
}

TEST(integration_unmapped_fault) {
    std::vector<uint32_t> prog;
    toy::Emit(&prog, toy::Lui(1, 0x1000)); // r1 = 0x10000000 (unmapped)
    toy::Emit(&prog, toy::Lw(2, 1, 0));
    toy::Emit(&prog, toy::Halt());

    std::string err;
    auto romPath = MakeRomPath("unmapped_fault");
    ASSERT_TRUE(rom::WriteRomU32LE(romPath, prog, &err));

    int rc = RunEmuWithRom(romPath, false, &err);
    ASSERT_EQ(rc, 0);
    EXPECT_TRUE(LastErrorIs(CpuErrorType::AccessFault));
}

TEST(integration_timer_smoke) {
    std::vector<uint32_t> prog;
    toy::Emit(&prog, toy::Lui(1, 0x2000));
    toy::Emit(&prog, toy::Ori(1, 0x1000)); // r1 = 0x20001000
    toy::Emit(&prog, toy::Lw(2, 1, 0));
    toy::Emit(&prog, toy::Lw(3, 1, 4));
    toy::Emit(&prog, toy::Sw(0, 1, 8)); // write CTRL reset
    toy::Emit(&prog, toy::Lw(4, 1, 0));
    toy::Emit(&prog, toy::Halt());

    std::string err;
    auto romPath = MakeRomPath("timer_smoke");
    ASSERT_TRUE(rom::WriteRomU32LE(romPath, prog, &err));

    int rc = RunEmuWithRom(romPath, false, &err);
    ASSERT_EQ(rc, 0);
    EXPECT_TRUE(LastErrorIs(CpuErrorType::Halt));
}
