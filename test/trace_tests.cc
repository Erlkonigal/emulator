#include "test_framework.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "emulator/debugger/debugger.h"
#include "emulator/device/device.h"
#include "emulator/device/memory.h"
#include "emulator/logging/logger.h"
#include "toy_cpu_executor.h"
#include "toy_isa.h"

namespace {

struct TraceTestContext {
    ToyCpuExecutor* Cpu = nullptr;
    MemoryBus* Bus = nullptr;
    MemoryDevice* Ram = nullptr;
    Debugger* Dbg = nullptr;
    std::string LogFile;
    std::function<std::string(const CommitInfo&)> Formatter;
    std::vector<std::string> TraceOutput;

    explicit TraceTestContext(const std::string& logFile) : LogFile(logFile) {
        logging::Config config;
        config.level = logging::Level::Trace;
        config.mFile = LogFile;
        logging::init(config);
        Cpu = new ToyCpuExecutor();
        Bus = new MemoryBus();
        Dbg = new Debugger(Cpu, Bus);
        Ram = new MemoryDevice(1024, false);
        Bus->registerDevice(Ram, 0, 1024);

        Bus->setDebugger(Dbg);
        Cpu->setDebugger(Dbg);
    }

    ~TraceTestContext() {
        delete Dbg;
        delete Cpu;
        delete Bus;
        delete Ram;
    }

    void RunSteps(int steps) {
        for (int i = 0; i < steps; ++i) {
            CycleResult result;
            Cpu->cycle(result);

            if (Formatter) {
                for (const auto& commit : result.commits) {
                    std::string line = Formatter(commit);
                    if (!line.empty()) {
                        TRACE("%s", line.c_str());
                        TraceOutput.push_back(line);
                    }
                }
            }
        }
    }

    void WriteProgram(const std::vector<uint32_t>& prog) {
        uint64_t addr = 0;
        for (uint32_t inst : prog) {
            MemAccess access;
            access.address = addr;
            access.size = 4;
            access.type = MemAccessType::Write;
            access.data = inst;
            Bus->write(access);
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

bool AnyLineContains(const std::vector<std::string>& lines, const std::string& needle) {
    for (const auto& line : lines) {
        if (line.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

void RegisterTraceTests() {
}

TEST(trace_custom_formatter) {
    std::string logFile = "/tmp/trace_custom_fmt.log";
    TraceTestContext ctx(logFile);

    ctx.Formatter = [&](const CommitInfo& commit) -> std::string {
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

    ctx.Formatter = [&](const CommitInfo& commit) -> std::string {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "PC:0x%08lx Inst:0x%08x %s",
                      commit.pc, commit.inst, commit.decoded.c_str());
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

    ctx.Formatter = [&](const CommitInfo& commit) -> std::string {
        std::stringstream ss;
        for (const auto& mem : commit.memEvents) {
            ss << "Mem:W:0x" << std::hex << mem.address << "=" << mem.data;
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

    ctx.Formatter = [&](const CommitInfo& commit) -> std::string {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "PC:0x%08lx Inst:0x%08x",
                      commit.pc, commit.inst);
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

    ctx.Formatter = [&](const CommitInfo& commit) -> std::string {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "PC:0x%08lx Inst:0x%08x %s",
                      commit.pc, commit.inst, commit.decoded.c_str());
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

    ctx.Formatter = [&](const CommitInfo& commit) -> std::string {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "PC:0x%08lx %s",
                      commit.pc, commit.decoded.c_str());
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
