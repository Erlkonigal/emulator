#ifndef TEST_TOY_CPU_EXECUTOR_H
#define TEST_TOY_CPU_EXECUTOR_H

#include <cstdint>

#include "emulator/cpu/cpu.h"

class ToyCpuExecutor;

ToyCpuExecutor* GetLastToyCpu();

class Debugger;

class ToyCpuExecutor : public ICpuExecutor {
public:
    ToyCpuExecutor();
    ~ToyCpuExecutor() override;

    void Reset() override;
    StepResult Step(uint64_t maxInstructions, uint64_t maxCycles) override;

    CpuErrorDetail GetLastError() const override;

    uint64_t GetPc() const override;
    void SetPc(uint64_t pc) override;
    uint64_t GetCycle() const override;

    uint64_t GetRegister(uint32_t regId) const override;
    void SetRegister(uint32_t regId, uint64_t value) override;

    void SetDebugger(ICpuDebugger* debugger) override;

    uint32_t GetRegisterCount() const override;

private:
    bool Fault(CpuErrorType type, uint64_t addr, uint32_t size);
    uint32_t FetchU32(uint64_t pc, MemResponse* out);

    ICpuDebugger* Dbg = nullptr;

    static constexpr uint32_t kRegCount = 16;
    uint64_t Regs[kRegCount] = {};
    uint64_t Pc = 0;
    uint64_t Cycle = 0;
    CpuErrorDetail LastError;
};

#endif
