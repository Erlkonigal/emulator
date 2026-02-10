#include "test_framework.h"

#include <string>
#include <vector>

#include "rom_util.h"
#include "stdout_capture.h"
#include "test_helpers.h"
#include "toy_cpu_executor.h"
#include "toy_isa.h"

void RegisterIntegrationTests() {
}

TEST(integration_uart_ok) {
    std::vector<uint32_t> prog;
    toy::Emit(&prog, toy::Lui(1, 0x2000));

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
    auto romPath = testutil::MakeRomPath("uart_ok");
    ASSERT_TRUE(rom::WriteRomU32LE(romPath, prog, &err));

    testutil::StdoutCapture cap;
    ASSERT_TRUE(cap.Start(&err));
    int rc = testutil::RunEmuWithRom(romPath, false, &err);
    std::string out;
    ASSERT_TRUE(cap.Stop(&out, &err));

    ASSERT_EQ(rc, 0);
    EXPECT_TRUE(testutil::LastErrorIs(CpuErrorType::None));
    EXPECT_TRUE(out.find("OK\n") != std::string::npos);
}

TEST(integration_ram_rw) {
    std::vector<uint32_t> prog;
    toy::Emit(&prog, toy::Lui(1, 0x8000));

    toy::Emit(&prog, toy::Lui(2, 0x1122));
    toy::Emit(&prog, toy::Ori(2, 0x3344));
    toy::Emit(&prog, toy::Sw(2, 1, 0));
    toy::Emit(&prog, toy::Lw(3, 1, 0));
    toy::Emit(&prog, toy::Halt());

    std::string err;
    auto romPath = testutil::MakeRomPath("ram_rw");
    ASSERT_TRUE(rom::WriteRomU32LE(romPath, prog, &err));

    int rc = testutil::RunEmuWithRom(romPath, false, &err);
    ASSERT_EQ(rc, 0);

    ToyCpuExecutor* cpu = GetLastToyCpu();
    ASSERT_TRUE(cpu != nullptr);
    EXPECT_TRUE(testutil::LastErrorIs(CpuErrorType::None));
    EXPECT_EQ(static_cast<uint32_t>(cpu->getRegister(3) & 0xffffffffu), 0x11223344u);
}

TEST(integration_unmapped_fault) {
    std::vector<uint32_t> prog;
    toy::Emit(&prog, toy::Lui(1, 0x1000));
    toy::Emit(&prog, toy::Lw(2, 1, 0));
    toy::Emit(&prog, toy::Halt());

    std::string err;
    auto romPath = testutil::MakeRomPath("unmapped_fault");
    ASSERT_TRUE(rom::WriteRomU32LE(romPath, prog, &err));

    int rc = testutil::RunEmuWithRom(romPath, false, &err);
    ASSERT_EQ(rc, 1);
    EXPECT_TRUE(testutil::LastErrorIs(CpuErrorType::AccessFault));
}

TEST(integration_timer_smoke) {
    std::vector<uint32_t> prog;
    toy::Emit(&prog, toy::Lui(1, 0x2000));
    toy::Emit(&prog, toy::Ori(1, 0x1000));
    toy::Emit(&prog, toy::Lw(2, 1, 0));
    toy::Emit(&prog, toy::Lw(3, 1, 4));
    toy::Emit(&prog, toy::Sw(0, 1, 8));
    toy::Emit(&prog, toy::Lw(4, 1, 0));
    toy::Emit(&prog, toy::Halt());

    std::string err;
    auto romPath = testutil::MakeRomPath("timer_smoke");
    ASSERT_TRUE(rom::WriteRomU32LE(romPath, prog, &err));

    int rc = testutil::RunEmuWithRom(romPath, false, &err);
    ASSERT_EQ(rc, 0);
    EXPECT_TRUE(testutil::LastErrorIs(CpuErrorType::None));
}
