#include "toy_cpu_executor.h"

#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

#include "emulator/debugger/debugger.h"

#include "toy_isa.h"

namespace {

ToyCpuExecutor* g_last = nullptr;

uint8_t OpCode(uint32_t inst) {
    return static_cast<uint8_t>((inst >> 24) & 0xff);
}

uint8_t Rd(uint32_t inst) {
    return static_cast<uint8_t>((inst >> 16) & 0xff);
}

uint8_t Rs(uint32_t inst) {
    return static_cast<uint8_t>((inst >> 8) & 0xff);
}

uint16_t Imm16(uint32_t inst) {
    return static_cast<uint16_t>(inst & 0xffffu);
}

int8_t Off8(uint32_t inst) {
    return static_cast<int8_t>(inst & 0xffu);
}

int64_t OffsetToWords(int8_t off) {
    return static_cast<int64_t>(off) * 4;
}

} // namespace

ToyCpuExecutor* GetLastToyCpu() {
    return g_last;
}

ToyCpuExecutor::ToyCpuExecutor() {
    g_last = this;
    Reset();
}

ToyCpuExecutor::~ToyCpuExecutor() = default;

void ToyCpuExecutor::Reset() {
    std::memset(Regs, 0, sizeof(Regs));
    Pc = 0;
    Cycle = 0;
    LastError = CpuErrorDetail{};
}

CpuErrorDetail ToyCpuExecutor::GetLastError() const {
    return LastError;
}

uint64_t ToyCpuExecutor::GetPc() const {
    return Pc;
}

void ToyCpuExecutor::SetPc(uint64_t pc) {
    Pc = pc;
}

uint64_t ToyCpuExecutor::GetCycle() const {
    return Cycle;
}

uint64_t ToyCpuExecutor::GetRegister(uint32_t regId) const {
    if (regId >= kRegCount) {
        return 0;
    }
    if (regId == 0) {
        return 0;
    }
    return Regs[regId];
}

void ToyCpuExecutor::SetRegister(uint32_t regId, uint64_t value) {
    if (regId >= kRegCount) {
        return;
    }
    if (regId == 0) {
        return;
    }
    Regs[regId] = value;
}

void ToyCpuExecutor::SetDebugger(ICpuDebugger* debugger) {
    Dbg = debugger;
}

uint32_t ToyCpuExecutor::GetRegisterCount() const {
    return kRegCount;
}

bool ToyCpuExecutor::Fault(CpuErrorType type, uint64_t addr, uint32_t size) {
    LastError.Type = type;
    LastError.Address = addr;
    LastError.Size = size;
    return false;
}

uint32_t ToyCpuExecutor::FetchU32(uint64_t pc, MemResponse* out) {
    if (out == nullptr || Dbg == nullptr) {
        return 0;
    }
    MemAccess access;
    access.Address = pc;
    access.Size = 4;
    access.Type = MemAccessType::Fetch;
    *out = Dbg->BusRead(access);
    if (!out->Success) {
        return 0;
    }
    return static_cast<uint32_t>(out->Data & 0xffffffffu);
}

StepResult ToyCpuExecutor::Step(uint64_t maxInstructions, uint64_t maxCycles) {
    StepResult result;
    result.Success = true;
    result.InstructionsExecuted = 0;
    result.CyclesExecuted = 0;

    if (Dbg == nullptr) {
        Fault(CpuErrorType::DeviceFault, Pc, 0);
        result.Success = false;
        return result;
    }

    auto traceOption = (Dbg) ? Dbg->GetTraceOptions() : TraceOptions{};
    bool hasBreakpoints = Dbg && Dbg->HasBreakpoints();

    while (result.InstructionsExecuted < maxInstructions && result.CyclesExecuted < maxCycles) {
        // Optimization: Cache flags

        bool logInstructions = traceOption.LogInstruction;
        bool logMemEvents = traceOption.LogMemEvents;
        bool logBranchPrediction = traceOption.LogBranchPrediction;

        if (hasBreakpoints && Dbg->IsBreakpoint(Pc)) {
             // Breakpoint hit, stop immediately
             return result;
        }

        // Prepare trace record
        TraceRecord record;
        record.Pc = Pc;
        record.CycleBegin = Cycle;
        // Toy ISA has no branches in this subset (maybe Jump is missing?), so always false
        record.IsBranch = false; 

        MemResponse fetch;
        uint32_t inst = FetchU32(Pc, &fetch);

        // Record fetch if applicable
        if (logMemEvents) {
            MemAccessEvent evt;
            evt.Type = MemAccessType::Fetch;
            evt.Address = Pc;
            evt.Size = 4;
            evt.Data = inst;
            evt.LatencyCycles = fetch.LatencyCycles;
            record.MemEvents.push_back(evt);
        }

        if (!fetch.Success) {
            LastError = fetch.Error;
            // Even on error, we might want to trace what happened so far, but PC hasn't moved.
            // Let's log if we have a sink.
            if (logMemEvents) {
                record.Inst = 0;
                record.Decoded = "FETCH_ERROR";
                record.CycleEnd = Cycle;
                Dbg->LogTrace(record);
            }
            result.Success = false;
            return result;
        }

        record.Inst = inst;
        uint64_t pcBefore = Pc;
        Pc += 4;
        ++Cycle;
        
        result.InstructionsExecuted++;
        result.CyclesExecuted++; // Base 1 cycle per instruction
        
        // Simple decoder for trace
        uint8_t op = OpCode(inst);
        bool success = true;

        if (op == static_cast<uint8_t>(toy::Op::Nop)) {
            if (logInstructions) record.Decoded = "NOP";
        } else if (op == static_cast<uint8_t>(toy::Op::Halt)) {
            if (logInstructions) record.Decoded = "HALT";
            success = Fault(CpuErrorType::Halt, pcBefore, 4);
        } else if (op == static_cast<uint8_t>(toy::Op::Lui)) {
            uint8_t rd = Rd(inst);
            uint16_t imm = Imm16(inst);
            if (logInstructions) record.Decoded = "LUI r" + std::to_string(rd) + ", " + std::to_string(imm);
            
            uint64_t value = static_cast<uint64_t>(imm) << 16;
            SetRegister(rd, value);
        } else if (op == static_cast<uint8_t>(toy::Op::Ori)) {
            uint8_t rd = Rd(inst);
            uint16_t imm = Imm16(inst);
            if (logInstructions) record.Decoded = "ORI r" + std::to_string(rd) + ", " + std::to_string(imm);

            uint64_t value = GetRegister(rd) | static_cast<uint64_t>(imm);
            SetRegister(rd, value);
        } else if (op == static_cast<uint8_t>(toy::Op::Beq)) {
            uint8_t r0 = Rd(inst);
            uint8_t r1 = Rs(inst);
            int8_t off = Off8(inst);
            if (logInstructions) {
                record.Decoded = "BEQ r" + std::to_string(r0) + ", r" + std::to_string(r1) +
                    ", " + std::to_string(off);
            }

            record.IsBranch = true;
            record.Branch.PredictedTaken = false;
            record.Branch.PredictedTarget = Pc + OffsetToWords(off);

            bool taken = GetRegister(r0) == GetRegister(r1);
            record.Branch.Taken = taken;
            record.Branch.Target = Pc + OffsetToWords(off);
            if (taken) {
                Pc = Pc + OffsetToWords(off);
            }
        } else if (op == static_cast<uint8_t>(toy::Op::Lw)) {
            uint8_t rd = Rd(inst);
            uint8_t rs = Rs(inst);
            int8_t off = Off8(inst);
            if (logInstructions) {
                record.Decoded = "LW r" + std::to_string(rd) + ", [r" + std::to_string(rs) + "+" + std::to_string(off) + "]";
            }

            uint64_t addr = GetRegister(rs) + static_cast<int64_t>(off);
            MemAccess access;
            access.Address = addr;
            access.Size = 4;
            access.Type = MemAccessType::Read;
            MemResponse r = Dbg->BusRead(access);

            if (logMemEvents) {
                MemAccessEvent evt;
                evt.Type = MemAccessType::Read;
                evt.Address = addr;
                evt.Size = 4;
                evt.Data = r.Data;
                evt.LatencyCycles = r.LatencyCycles;
                record.MemEvents.push_back(evt);
            }

            if (!r.Success) {
                LastError = r.Error;
                success = false;
            } else {
                SetRegister(rd, static_cast<uint32_t>(r.Data & 0xffffffffu));
            }
        } else if (op == static_cast<uint8_t>(toy::Op::Sw)) {
            uint8_t rs = Rd(inst); // Source register for store is in Rd position in encode
            uint8_t rd = Rs(inst); // Base address register
            int8_t off = Off8(inst);
            if (logInstructions) {
                record.Decoded = "SW r" + std::to_string(rs) + ", [r" + std::to_string(rd) + "+" + std::to_string(off) + "]";
            }

            uint64_t addr = GetRegister(rd) + static_cast<int64_t>(off);
            MemAccess access;
            access.Address = addr;
            access.Size = 4;
            access.Type = MemAccessType::Write;
            access.Data = static_cast<uint32_t>(GetRegister(rs) & 0xffffffffu);
            MemResponse w = Dbg->BusWrite(access);

            if (logMemEvents) {
                MemAccessEvent evt;
                evt.Type = MemAccessType::Write;
                evt.Address = addr;
                evt.Size = 4;
                evt.Data = access.Data;
                evt.LatencyCycles = w.LatencyCycles;
                record.MemEvents.push_back(evt);
            }

            if (!w.Success) {
                LastError = w.Error;
                success = false;
            }
        } else {
            if (logInstructions) record.Decoded = "INVALID_OP";
            success = Fault(CpuErrorType::InvalidOp, pcBefore, 4);
        }

        record.CycleEnd = Cycle;
        if (logInstructions || logBranchPrediction || logMemEvents) Dbg->LogTrace(record);

        if (!success) {
            result.Success = false;
            return result;
        }
    }
    
    return result;
}

extern "C" ICpuExecutor* CreateCpuExecutor() {
    static ToyCpuExecutor cpu;
    return &cpu;
}
