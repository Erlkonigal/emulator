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
    CpuErrorType errorType = CpuErrorType::None;
};

struct MemAccessEvent {
    MemAccessType type = MemAccessType::Read;
    uint64_t address = 0;
    uint32_t size = 0;
    uint64_t data = 0;
    uint32_t latencyCycles = 0;
};

struct RegisterEvent {
    uint32_t regId = 0;
    uint64_t newValue = 0;
};

struct BranchDetails {
    bool taken = false;
    uint64_t target = 0;
    bool predictedTaken = false;
    uint64_t predictedTarget = 0;
};

struct CommitInfo {
    uint64_t pc = 0;
    uint32_t inst = 0;
    std::string decoded;
    std::vector<MemAccessEvent> memEvents;
    std::vector<RegisterEvent> regEvents;
    bool isBranch = false;
    BranchDetails branch;
};

struct ErrorInfo {
    CpuErrorType type = CpuErrorType::None;
    std::string message;
    uint64_t pc = 0;
};

struct CycleResult {
    std::vector<CommitInfo> commits;
    std::vector<ErrorInfo> errors;
};

class TraceSink;

class ICpuDebugger {
public:
    virtual ~ICpuDebugger() = default;

    virtual MemResponse busRead(const MemAccess& access) = 0;
    virtual MemResponse busWrite(const MemAccess& access) = 0;
    virtual bool isBreakpoint(uint64_t address) = 0;
    virtual bool hasBreakpoints() = 0;
};

class ICpuExecutor {
public:
    virtual ~ICpuExecutor() = default;

    virtual void reset() = 0;
    virtual void cycle(CycleResult& result) = 0;

    virtual uint64_t getPc() const = 0;
    virtual void setPc(uint64_t pc) = 0;
    virtual uint64_t getCycle() const = 0;

    virtual uint64_t getRegister(uint32_t regId) const = 0;
    virtual void setRegister(uint32_t regId, uint64_t value) = 0;

    virtual void setDebugger(ICpuDebugger* debugger) = 0;

    virtual uint32_t getRegisterCount() const = 0;
};

#endif
