#ifndef EMULATOR_CPU_CPU_H
#define EMULATOR_CPU_CPU_H

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

class MemoryBus;

enum class CpuState {
    Running,
    Pause,
    Halted
};

enum class CpuErrorType {
    None,
    InvalidOp,
    AccessFault,
    DeviceFault,
    Halt
};

struct CpuErrorDetail {
    CpuErrorType type = CpuErrorType::None;
    uint64_t address = 0;
    uint32_t size = 0;
    uint64_t data = 0;
};

enum class MemAccessType {
    Read,
    Write,
    Fetch
};

struct MemAccess {
    uint64_t address = 0;
    uint32_t size = 0;
    MemAccessType type = MemAccessType::Read;
    uint64_t data = 0;
};

struct MemResponse {
    bool success = true;
    uint64_t data = 0;
    uint32_t latencyCycles = 0;
    CpuErrorDetail error;
};

struct StepResult {
    bool success = true;
    uint64_t instructionsExecuted = 0;
    uint64_t cyclesExecuted = 0;
};

struct MemAccessEvent {
    MemAccessType type = MemAccessType::Read;
    uint64_t address = 0;
    uint32_t size = 0;
    uint64_t data = 0;
    uint32_t latencyCycles = 0;
};

struct BranchDetails {
    bool taken = false;
    uint64_t target = 0;
    bool predictedTaken = false;
    uint64_t predictedTarget = 0;
};

struct TraceRecord {
    uint64_t pc = 0;
    uint32_t inst = 0;
    std::string decoded;
    uint64_t cycleBegin = 0;
    uint64_t cycleEnd = 0;
    std::vector<MemAccessEvent> memEvents;
    bool isBranch = false;
    BranchDetails branch;
    std::vector<std::pair<std::string, std::string>> extra;
};

struct TraceOptions {
    bool logInstruction = true;
    bool logMemEvents = true;
    bool logBranchPrediction = true;
};

using TraceFormatter = std::function<std::string(const TraceRecord&, const TraceOptions&)>;

class TraceSink;

class ICpuDebugger {
public:
    virtual ~ICpuDebugger() = default;

    virtual MemResponse busRead(const MemAccess& access) = 0;
    virtual MemResponse busWrite(const MemAccess& access) = 0;
    virtual bool isBreakpoint(uint64_t address) = 0;
    virtual bool hasBreakpoints() = 0;

    virtual void configureTrace(const TraceOptions& options) = 0;
    virtual void setTraceFormatter(TraceFormatter formatter) = 0;
    virtual void logTrace(const TraceRecord& record) = 0;
    virtual const TraceOptions& getTraceOptions() const = 0;
};

class ICpuExecutor {
public:
    virtual ~ICpuExecutor() = default;

    virtual void reset() = 0;
    virtual StepResult step(uint64_t maxInstructions, uint64_t maxCycles) = 0;

    virtual CpuErrorDetail getLastError() const = 0;

    virtual uint64_t getPc() const = 0;
    virtual void setPc(uint64_t pc) = 0;
    virtual uint64_t getCycle() const = 0;

    virtual uint64_t getRegister(uint32_t regId) const = 0;
    virtual void setRegister(uint32_t regId, uint64_t value) = 0;

    virtual void setDebugger(ICpuDebugger* debugger) = 0;

    virtual uint32_t getRegisterCount() const = 0;
};

#endif
