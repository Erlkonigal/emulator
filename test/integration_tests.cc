#include "test_framework.h"

#include <string>
#include <vector>

#include "rom_util.h"
#include "stdout_capture.h"
#include "test_helpers.h"
#include "toy_cpu_executor.h"
#include "toy_isa.h"

void RegisterIntegrationTests() {}

TEST(integration_ram_rw) {
  std::vector<uint32_t> prog;

  toy::Emit(&prog, toy::Lui(1, 0x0));
  toy::Emit(&prog, toy::Ori(1, 0x100));
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

  ToyCpuExecutor *cpu = GetLastToyCpu();
  ASSERT_TRUE(cpu != nullptr);
  EXPECT_EQ(static_cast<uint32_t>(cpu->getRegister(3) & 0xffffffffu),
            0x11223344u);
}

TEST(integration_basic_compute) {
  std::vector<uint32_t> prog;

  toy::Emit(&prog, toy::Lui(1, 0x1234));
  toy::Emit(&prog, toy::Ori(1, 0x5678));
  toy::Emit(&prog, toy::Halt());

  std::string err;
  auto romPath = testutil::MakeRomPath("basic_compute");
  ASSERT_TRUE(rom::WriteRomU32LE(romPath, prog, &err));

  int rc = testutil::RunEmuWithRom(romPath, false, &err);
  ASSERT_EQ(rc, 0);

  ToyCpuExecutor *cpu = GetLastToyCpu();
  ASSERT_TRUE(cpu != nullptr);
  EXPECT_EQ(static_cast<uint32_t>(cpu->getRegister(1) & 0xffffffffu),
            0x12345678u);
}

TEST(integration_beq_taken) {
  std::vector<uint32_t> prog;

  toy::Emit(&prog, toy::Lui(1, 0x0001));
  toy::Emit(&prog, toy::Lui(2, 0x0001));
  toy::Emit(&prog, toy::Beq(1, 2, 1));
  toy::Emit(&prog, toy::Lui(3, 0x0000));
  toy::Emit(&prog, toy::Lui(3, 0x0001));
  toy::Emit(&prog, toy::Halt());

  std::string err;
  auto romPath = testutil::MakeRomPath("beq_taken");
  ASSERT_TRUE(rom::WriteRomU32LE(romPath, prog, &err));

  int rc = testutil::RunEmuWithRom(romPath, false, &err);
  ASSERT_EQ(rc, 0);

  ToyCpuExecutor *cpu = GetLastToyCpu();
  ASSERT_TRUE(cpu != nullptr);
}

TEST(integration_halt) {
  std::vector<uint32_t> prog;

  toy::Emit(&prog, toy::Halt());

  std::string err;
  auto romPath = testutil::MakeRomPath("halt");
  ASSERT_TRUE(rom::WriteRomU32LE(romPath, prog, &err));

  int rc = testutil::RunEmuWithRom(romPath, false, &err);
  ASSERT_EQ(rc, 0);

  ToyCpuExecutor *cpu = GetLastToyCpu();
  ASSERT_TRUE(cpu != nullptr);
  EXPECT_TRUE(testutil::CheckLastError(CpuErrorType::Halt));
}