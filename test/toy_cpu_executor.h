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

    void reset() override;
    StepResult step(uint64_t maxInstructions, uint64_t maxCycles) override;

    CpuErrorDetail getLastError() const override;

    uint64_t getPc() const override;
    void setPc(uint64_t pc) override;
    uint64_t getCycle() const override;

    uint64_t getRegister(uint32_t regId) const override;
    void setRegister(uint32_t regId, uint64_t value) override;

    void setDebugger(ICpuDebugger* debugger) override;

    uint32_t getRegisterCount() const override;

private:
    bool fault(CpuErrorType type, uint64_t addr, uint32_t size);
    uint32_t fetchU32(uint64_t pc, MemResponse* out);

    ICpuDebugger* mDbg = nullptr;

    static constexpr uint32_t kRegCount = 16;
    uint64_t mRegs[kRegCount] = {};
    uint64_t mPc = 0;
    uint64_t mCycle = 0;
    CpuErrorDetail mLastError;
};

#endif
