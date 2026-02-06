#ifndef EMULATOR_DEBUGGER_H
#define EMULATOR_DEBUGGER_H

#include "emulator/bus.h"
#include "emulator/cpu.h"


class Debugger {
public:
    Debugger(ICpuExecutor* cpu, MemoryBus* bus);

    void RunUntilHalt();
    void StepInstruction();
    std::vector<uint8_t> ScanMemory(uint64_t address, uint32_t length);
    std::vector<uint64_t> ReadRegisters();
    void PrintRegisters();
    uint64_t EvalExpression(const std::string& expression);
    void AddBreakpoint(uint64_t address);
    void RemoveBreakpoint(uint64_t address);
    bool ProcessCommand(const std::string& command);

    void SetRegisterCount(uint32_t count);

private:
    ICpuExecutor* Cpu = nullptr;
    MemoryBus* Bus = nullptr;
    uint32_t RegisterCount = 0;
    std::vector<uint64_t> Breakpoints;
};

#endif
