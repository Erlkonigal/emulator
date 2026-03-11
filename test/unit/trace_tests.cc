#include "framework/test_framework.h"

#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "emulator/device/ram.h"
#include "emulator/device/rom.h"
#include "emulator/log/tracer.h"
#include "emulator/log/trace_manager.h"
#include "toy/toy_cpu_executor.h"
#include "toy/toy_isa.h"

namespace {

uint8_t OpCode(uint32_t inst) {
  return static_cast<uint8_t>((inst >> 24) & 0xff);
}
uint8_t Rd(uint32_t inst) { return static_cast<uint8_t>((inst >> 16) & 0xff); }
uint8_t Rs(uint32_t inst) { return static_cast<uint8_t>((inst >> 8) & 0xff); }
uint16_t Imm16(uint32_t inst) { return static_cast<uint16_t>(inst & 0xffffu); }
int8_t Off8(uint32_t inst) { return static_cast<int8_t>(inst & 0xffu); }

std::string Decode(uint32_t inst) {
  uint8_t op = OpCode(inst);
  if (op == static_cast<uint8_t>(toy::Op::Nop)) {
    return "NOP";
  } else if (op == static_cast<uint8_t>(toy::Op::Halt)) {
    return "HALT";
  } else if (op == static_cast<uint8_t>(toy::Op::Lui)) {
    uint8_t rd = Rd(inst);
    uint16_t imm = Imm16(inst);
    return "LUI r" + std::to_string(rd) + ", " + std::to_string(imm);
  } else if (op == static_cast<uint8_t>(toy::Op::Ori)) {
    uint8_t rd = Rd(inst);
    uint16_t imm = Imm16(inst);
    return "ORI r" + std::to_string(rd) + ", " + std::to_string(imm);
  } else if (op == static_cast<uint8_t>(toy::Op::Beq)) {
    uint8_t r0 = Rd(inst);
    uint8_t r1 = Rs(inst);
    int8_t off = Off8(inst);
    return "BEQ r" + std::to_string(r0) + ", r" + std::to_string(r1) + ", " +
           std::to_string(off);
  } else if (op == static_cast<uint8_t>(toy::Op::Lw)) {
    uint8_t rd = Rd(inst);
    uint8_t rs = Rs(inst);
    int8_t off = Off8(inst);
    return "LW r" + std::to_string(rd) + ", [r" + std::to_string(rs) + "+" +
           std::to_string(off) + "]";
  } else if (op == static_cast<uint8_t>(toy::Op::Sw)) {
    uint8_t rd = Rd(inst);
    uint8_t rs = Rs(inst);
    int8_t off = Off8(inst);
    return "SW r" + std::to_string(rd) + ", [r" + std::to_string(rs) + "+" +
           std::to_string(off) + "]";
  }
  return "INVALID_OP";
}

struct TraceTestContext {
  std::unique_ptr<ToyCpuExecutor> Cpu;
  std::function<std::string(const CommitInfo &)> Formatter;
  std::vector<std::string> TraceOutput;
  Tracer tracer;

  explicit TraceTestContext() {
    tracer.init({.name = "TEST", .handler = nullptr});

    auto& ram = Ram::getInstance();
    ram.init(65536);

    Cpu = std::make_unique<ToyCpuExecutor>();
  }

  void RunSteps(int steps) {
    for (int i = 0; i < steps; ++i) {
      CommitArray commits;
      Cpu->cycle(commits);

      if (Formatter) {
        for (const auto &commit : commits) {
          if (!commit.valid)
            continue;
          std::string line = Formatter(commit);
          if (!line.empty()) {
            tracer.trace("%s", line.c_str());
            TraceOutput.push_back(line);
          }
        }
      }
    }
  }

  void WriteProgram(const std::vector<uint32_t> &prog) {
    std::vector<uint8_t> romData(prog.size() * 4);
    for (size_t i = 0; i < prog.size(); ++i) {
      romData[i * 4] = prog[i] & 0xff;
      romData[i * 4 + 1] = (prog[i] >> 8) & 0xff;
      romData[i * 4 + 2] = (prog[i] >> 16) & 0xff;
      romData[i * 4 + 3] = (prog[i] >> 24) & 0xff;
    }
    Rom::getInstance().init(romData);
  }
};

bool AnyLineContains(const std::vector<std::string> &lines,
                     const std::string &needle) {
  for (const auto &line : lines) {
    if (line.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

} // namespace

void RegisterTraceTests() {}

TEST(trace_custom_formatter) {
  TraceTestContext ctx;

  ctx.Formatter = [&](const CommitInfo &commit) -> std::string {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "CUSTOM: 0x%lx %lx", commit.pc, commit.inst);
    return std::string(buf);
  };

  std::vector<uint32_t> prog;
  toy::Emit(&prog, toy::Nop());
  ctx.WriteProgram(prog);
  ctx.RunSteps(1);

  EXPECT_TRUE(AnyLineContains(ctx.TraceOutput, "CUSTOM: 0x0 0"));
}

TEST(trace_itrace_only) {
  TraceTestContext ctx;

  ctx.Formatter = [&](const CommitInfo &commit) -> std::string {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "PC:0x%08lx Inst:0x%08lx %s", commit.pc,
                  commit.inst, Decode(commit.inst).c_str());
    return std::string(buf);
  };

  std::vector<uint32_t> prog;
  toy::Emit(&prog, toy::Nop());
  ctx.WriteProgram(prog);
  ctx.RunSteps(1);

  EXPECT_TRUE(AnyLineContains(ctx.TraceOutput, "PC:0x00000000"));
  EXPECT_TRUE(AnyLineContains(ctx.TraceOutput, "NOP"));
  EXPECT_TRUE(!AnyLineContains(ctx.TraceOutput, "Mem"));
}

TEST(trace_itrace_mtrace_combo) {
  TraceTestContext ctx;

  ctx.Formatter = [&](const CommitInfo &commit) -> std::string {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "PC:0x%08lx Inst:0x%08lx", commit.pc,
                  commit.inst);
    return std::string(buf);
  };

  std::vector<uint32_t> prog;
  toy::Emit(&prog, toy::Lui(1, 0x8000));
  toy::Emit(&prog, toy::Sw(0, 1, 0));
  ctx.WriteProgram(prog);
  ctx.RunSteps(2);

  EXPECT_TRUE(AnyLineContains(ctx.TraceOutput, "PC:0x00000000"));
}

TEST(trace_all_enabled) {
  TraceTestContext ctx;

  ctx.Formatter = [&](const CommitInfo &commit) -> std::string {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "PC:0x%08lx %s", commit.pc,
                  Decode(commit.inst).c_str());
    return std::string(buf);
  };

  std::vector<uint32_t> prog;
  toy::Emit(&prog, toy::Lui(1, 0x8000));
  toy::Emit(&prog, toy::Lui(2, 0x8000));
  toy::Emit(&prog, toy::Beq(1, 2, 1));
  toy::Emit(&prog, toy::Nop());
  toy::Emit(&prog, toy::Sw(0, 1, 0));
  ctx.WriteProgram(prog);
  ctx.RunSteps(4);

  EXPECT_TRUE(AnyLineContains(ctx.TraceOutput, "BEQ"));
}