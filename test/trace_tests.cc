#include "test_framework.h"

#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "emulator/log/logger.h"
#include "toy_cpu_executor.h"
#include "toy_isa.h"

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
  std::string LogFile;
  std::function<std::string(const CommitInfo &)> Formatter;
  std::vector<std::string> TraceOutput;

  explicit TraceTestContext(const std::string &logFile) : LogFile(logFile) {
    logging::Config config;
    config.level = logging::Level::Trace;
    config.mFile = LogFile;
    logging::init(config);

    Cpu = std::make_unique<ToyCpuExecutor>();
  }

  ~TraceTestContext() = default;

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
            TRACE("%s", line.c_str());
            TraceOutput.push_back(line);
          }
        }
      }
    }
  }

  void WriteProgram(const std::vector<uint32_t> &prog) {
    uint64_t addr = 0;
    for (uint32_t inst : prog) {
      Cpu->writeMem(addr, inst);
      addr += 4;
    }
  }

  std::vector<std::string> ReadLog() const {
    std::vector<std::string> lines;
    std::ifstream f(LogFile);
    std::string line;
    while (std::getline(f, line)) {
      lines.push_back(line);
    }
    return lines;
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
  std::string logFile = "/tmp/trace_custom_fmt.log";
  TraceTestContext ctx(logFile);

  ctx.Formatter = [&](const CommitInfo &commit) -> std::string {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "CUSTOM: 0x%lx %x", commit.pc, commit.inst);
    return std::string(buf);
  };

  std::vector<uint32_t> prog;
  toy::Emit(&prog, toy::Nop());
  ctx.WriteProgram(prog);
  ctx.RunSteps(1);

  EXPECT_TRUE(AnyLineContains(ctx.TraceOutput, "CUSTOM: 0x0 0"));
  std::remove(logFile.c_str());
}

TEST(trace_itrace_only) {
  std::string logFile = "/tmp/trace_itrace.log";
  TraceTestContext ctx(logFile);

  ctx.Formatter = [&](const CommitInfo &commit) -> std::string {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "PC:0x%08lx Inst:0x%08x %s", commit.pc,
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
  std::remove(logFile.c_str());
}

TEST(trace_mtrace_only) {
  std::string logFile = "/tmp/trace_mtrace.log";
  TraceTestContext ctx(logFile);

  ctx.Formatter = [&](const CommitInfo &commit) -> std::string {
    std::stringstream ss;
    if (commit.isMemWrite) {
      ss << "Mem:W:0x" << std::hex << commit.memAddress << "="
         << commit.memData;
    }
    return ss.str();
  };

  std::vector<uint32_t> prog;
  toy::Emit(&prog, toy::Sw(0, 0, 4));
  ctx.WriteProgram(prog);
  ctx.RunSteps(1);

  EXPECT_TRUE(AnyLineContains(ctx.TraceOutput, "Mem:W:0x4="));
  std::remove(logFile.c_str());
}

TEST(trace_itrace_mtrace_combo) {
  std::string logFile = "/tmp/trace_imtrace.log";
  TraceTestContext ctx(logFile);

  ctx.Formatter = [&](const CommitInfo &commit) -> std::string {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "PC:0x%08lx Inst:0x%08x", commit.pc,
                  commit.inst);
    return std::string(buf);
  };

  std::vector<uint32_t> prog;
  toy::Emit(&prog, toy::Lui(1, 0x8000));
  toy::Emit(&prog, toy::Sw(0, 1, 0));
  ctx.WriteProgram(prog);
  ctx.RunSteps(2);

  EXPECT_TRUE(AnyLineContains(ctx.TraceOutput, "PC:0x00000000"));
  std::remove(logFile.c_str());
}

TEST(trace_bptrace) {
  std::string logFile = "/tmp/trace_bptrace.log";
  TraceTestContext ctx(logFile);

  ctx.Formatter = [&](const CommitInfo &commit) -> std::string {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "PC:0x%08lx Inst:0x%08x %s", commit.pc,
                  commit.inst, Decode(commit.inst).c_str());
    return std::string(buf);
  };

  std::vector<uint32_t> prog;
  toy::Emit(&prog, toy::Lui(1, 0x1));
  toy::Emit(&prog, toy::Lui(2, 0x1));
  toy::Emit(&prog, toy::Beq(1, 2, 1));
  toy::Emit(&prog, toy::Nop());
  ctx.WriteProgram(prog);
  ctx.RunSteps(3);

  EXPECT_TRUE(AnyLineContains(ctx.TraceOutput, "BEQ r1, r2, 1"));
  std::remove(logFile.c_str());
}

TEST(trace_all_enabled) {
  std::string logFile = "/tmp/trace_all.log";
  TraceTestContext ctx(logFile);

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
  std::remove(logFile.c_str());
}