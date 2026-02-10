#include "test_framework.h"

#include <cstdio>
#include <fstream>
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
        Cpu->step(steps, 1000000);
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
    std::string logFile = "test_custom_fmt.log";
    TraceTestContext ctx(logFile);

    ctx.Dbg->setTraceFormatter([&](const TraceRecord& record, const TraceOptions&) -> std::string {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "CUSTOM: 0x%lx %x", record.pc, record.inst);
        return std::string(buf);
    });

    TraceOptions opts;
    opts.logInstruction = true;
    opts.logMemEvents = false;
    opts.logBranchPrediction = false;
    ctx.Dbg->configureTrace(opts);

    std::vector<uint32_t> prog;
    toy::Emit(&prog, toy::Nop());
    ctx.WriteProgram(prog);
    ctx.RunSteps(1);

    auto lines = ctx.ReadLog();
    EXPECT_TRUE(AnyLineContains(lines, "CUSTOM: 0x0 0"));
    std::remove(logFile.c_str());
}

TEST(trace_itrace_only) {
    std::string logFile = "test_itrace.log";
    TraceTestContext ctx(logFile);

    TraceOptions opts;
    opts.logInstruction = true;
    opts.logMemEvents = false;
    opts.logBranchPrediction = false;
    ctx.Dbg->configureTrace(opts);

    std::vector<uint32_t> prog;
    toy::Emit(&prog, toy::Nop());
    ctx.WriteProgram(prog);
    ctx.RunSteps(1);

    auto lines = ctx.ReadLog();
    EXPECT_TRUE(AnyLineContains(lines, "PC:0x00000000"));
    EXPECT_TRUE(AnyLineContains(lines, "(NOP)"));
    EXPECT_TRUE(!AnyLineContains(lines, "Mem:["));
    std::remove(logFile.c_str());
}

TEST(trace_mtrace_only) {
    std::string logFile = "test_mtrace.log";
    TraceTestContext ctx(logFile);

    TraceOptions opts;
    opts.logInstruction = false;
    opts.logMemEvents = true;
    opts.logBranchPrediction = false;
    ctx.Dbg->configureTrace(opts);

    std::vector<uint32_t> prog;
    toy::Emit(&prog, toy::Sw(0, 0, 4));
    ctx.WriteProgram(prog);
    ctx.RunSteps(1);

    auto lines = ctx.ReadLog();
    EXPECT_TRUE(AnyLineContains(lines, "Mem:[W:0x4="));
    EXPECT_TRUE(!AnyLineContains(lines, "PC:0x"));
    std::remove(logFile.c_str());
}

TEST(trace_itrace_mtrace_combo) {
    std::string logFile = "test_imtrace.log";
    TraceTestContext ctx(logFile);

    TraceOptions opts;
    opts.logInstruction = true;
    opts.logMemEvents = true;
    opts.logBranchPrediction = false;
    ctx.Dbg->configureTrace(opts);

    std::vector<uint32_t> prog;
    toy::Emit(&prog, toy::Lui(1, 0x8000));
    toy::Emit(&prog, toy::Sw(0, 1, 0));
    ctx.WriteProgram(prog);
    ctx.RunSteps(2);

    auto lines = ctx.ReadLog();
    EXPECT_TRUE(AnyLineContains(lines, "PC:0x00000000"));
    EXPECT_TRUE(AnyLineContains(lines, "Mem:[W:0x80000000="));
    std::remove(logFile.c_str());
}

TEST(trace_bptrace) {
    std::string logFile = "test_bptrace.log";
    TraceTestContext ctx(logFile);

    TraceOptions opts;
    opts.logInstruction = true;
    opts.logMemEvents = false;
    opts.logBranchPrediction = true;
    ctx.Dbg->configureTrace(opts);

    std::vector<uint32_t> prog;
    toy::Emit(&prog, toy::Lui(1, 0x1));
    toy::Emit(&prog, toy::Lui(2, 0x1));
    toy::Emit(&prog, toy::Beq(1, 2, 1));
    toy::Emit(&prog, toy::Nop());
    ctx.WriteProgram(prog);
    ctx.RunSteps(3);

    auto lines = ctx.ReadLog();
    EXPECT_TRUE(AnyLineContains(lines, "(BEQ r1, r2"));
    EXPECT_TRUE(AnyLineContains(lines, "BP:(T:1"));
    std::remove(logFile.c_str());
}

TEST(trace_all_enabled) {
    std::string logFile = "test_alltrace.log";
    TraceTestContext ctx(logFile);

    TraceOptions opts;
    opts.logInstruction = true;
    opts.logMemEvents = true;
    opts.logBranchPrediction = true;
    ctx.Dbg->configureTrace(opts);

    std::vector<uint32_t> prog;
    toy::Emit(&prog, toy::Lui(1, 0x8000));
    toy::Emit(&prog, toy::Lui(2, 0x8000));
    toy::Emit(&prog, toy::Beq(1, 2, 1));
    toy::Emit(&prog, toy::Nop());
    toy::Emit(&prog, toy::Sw(0, 1, 0));
    ctx.WriteProgram(prog);
    ctx.RunSteps(4);

    auto lines = ctx.ReadLog();
    EXPECT_TRUE(AnyLineContains(lines, "BP:(T:1"));
    EXPECT_TRUE(AnyLineContains(lines, "Mem:[W:0x80000000="));
    std::remove(logFile.c_str());
}
