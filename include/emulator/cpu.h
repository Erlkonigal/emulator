#ifndef EMULATOR_CPU_H
#define EMULATOR_CPU_H

#include <cstdint>
#include <string>
#include <vector>

class MemoryBus;
class TraceSink;

enum class CpuState {
    Start,
    Running,
    Pause,
    Finish
};

enum class CpuResult {
    Success,
    Error
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
    CpuResult Result = CpuResult::Success;
    uint64_t Data = 0;
    uint32_t LatencyCycles = 0;
    CpuErrorDetail Error;
};

class ICpuExecutor {
public:
    virtual ~ICpuExecutor() = default;

    virtual void Reset() = 0;
    virtual CpuResult StepInstruction() = 0;

    virtual CpuErrorDetail GetLastError() const = 0;

    virtual uint64_t GetPc() const = 0;
    virtual void SetPc(uint64_t pc) = 0;
    virtual uint64_t GetCycle() const = 0;

    virtual uint64_t GetRegister(uint32_t regId) const = 0;
    virtual void SetRegister(uint32_t regId, uint64_t value) = 0;

    virtual void SetMemoryBus(MemoryBus* bus) = 0;
    virtual void SetTraceSink(TraceSink* traceSink) = 0;

    virtual uint32_t GetRegisterCount() const = 0;
};

#endif
