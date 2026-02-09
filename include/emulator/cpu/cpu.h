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
    CpuErrorType Type = CpuErrorType::None;
    uint64_t Address = 0;
    uint32_t Size = 0;
    uint64_t Data = 0;
};

enum class MemAccessType {
    Read,
    Write,
    Fetch
};

struct MemAccess {
    uint64_t Address = 0;
    uint32_t Size = 0;
    MemAccessType Type = MemAccessType::Read;
    uint64_t Data = 0;
};

struct MemResponse {
    bool Success = true;
    uint64_t Data = 0;
    uint32_t LatencyCycles = 0;
    CpuErrorDetail Error;
};

struct StepResult {
    bool Success = true;
    uint64_t InstructionsExecuted = 0;
    uint64_t CyclesExecuted = 0;
};

struct MemAccessEvent {
    MemAccessType Type = MemAccessType::Read;
    uint64_t Address = 0;
    uint32_t Size = 0;
    uint64_t Data = 0;
    uint32_t LatencyCycles = 0;
};

struct BranchDetails {
    bool Taken = false;
    uint64_t Target = 0;
    bool PredictedTaken = false;
    uint64_t PredictedTarget = 0;
};

struct TraceRecord {
    uint64_t Pc = 0;
    uint32_t Inst = 0;
    std::string Decoded;
    uint64_t CycleBegin = 0;
    uint64_t CycleEnd = 0;
    std::vector<MemAccessEvent> MemEvents;
    bool IsBranch = false;
    BranchDetails Branch;
    std::vector<std::pair<std::string, std::string>> Extra;
};

struct TraceOptions {
    bool LogInstruction = true;
    bool LogMemEvents = true;
    bool LogBranchPrediction = true;
};

using TraceFormatter = std::function<std::string(const TraceRecord&, const TraceOptions&)>;

class TraceSink;

class ICpuDebugger {
public:
    virtual ~ICpuDebugger() = default;

    virtual MemResponse BusRead(const MemAccess& access) = 0;
    virtual MemResponse BusWrite(const MemAccess& access) = 0;
    virtual bool IsBreakpoint(uint64_t address) = 0;
    virtual bool HasBreakpoints() = 0;

    virtual void ConfigureTrace(const TraceOptions& options) = 0;
    virtual void SetTraceFormatter(TraceFormatter formatter) = 0;
    virtual void LogTrace(const TraceRecord& record) = 0;
    virtual const TraceOptions& GetTraceOptions() const = 0;
};

class ICpuExecutor {
public:
    virtual ~ICpuExecutor() = default;

    virtual void Reset() = 0;
    virtual StepResult Step(uint64_t maxInstructions, uint64_t maxCycles) = 0;

    virtual CpuErrorDetail GetLastError() const = 0;

    virtual uint64_t GetPc() const = 0;
    virtual void SetPc(uint64_t pc) = 0;
    virtual uint64_t GetCycle() const = 0;

    virtual uint64_t GetRegister(uint32_t regId) const = 0;
    virtual void SetRegister(uint32_t regId, uint64_t value) = 0;

    virtual void SetDebugger(ICpuDebugger* debugger) = 0;

    virtual uint32_t GetRegisterCount() const = 0;
};

#endif
