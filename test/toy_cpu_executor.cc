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
    reset();
}

ToyCpuExecutor::~ToyCpuExecutor() = default;

void ToyCpuExecutor::reset() {
    std::memset(mRegs, 0, sizeof(mRegs));
    mPc = 0;
    mCycle = 0;
    mLastError = CpuErrorDetail{};
}

CpuErrorDetail ToyCpuExecutor::getLastError() const {
    return mLastError;
}

uint64_t ToyCpuExecutor::getPc() const {
    return mPc;
}

void ToyCpuExecutor::setPc(uint64_t pc) {
    mPc = pc;
}

uint64_t ToyCpuExecutor::getCycle() const {
    return mCycle;
}

uint64_t ToyCpuExecutor::getRegister(uint32_t regId) const {
    if (regId >= kRegCount) {
        return 0;
    }
    if (regId == 0) {
        return 0;
    }
    return mRegs[regId];
}

void ToyCpuExecutor::setRegister(uint32_t regId, uint64_t value) {
    if (regId >= kRegCount) {
        return;
    }
    if (regId == 0) {
        return;
    }
    mRegs[regId] = value;
}

void ToyCpuExecutor::setDebugger(ICpuDebugger* debugger) {
    mDbg = debugger;
}

uint32_t ToyCpuExecutor::getRegisterCount() const {
    return kRegCount;
}

bool ToyCpuExecutor::fault(CpuErrorType type, uint64_t addr, uint32_t size) {
    mLastError.type = type;
    mLastError.address = addr;
    mLastError.size = size;
    return false;
}

uint32_t ToyCpuExecutor::fetchU32(uint64_t pc, MemResponse* out) {
    if (out == nullptr || mDbg == nullptr) {
        return 0;
    }
    MemAccess access;
    access.address = pc;
    access.size = 4;
    access.type = MemAccessType::Fetch;
    *out = mDbg->busRead(access);
    if (!out->success) {
        return 0;
    }
    return static_cast<uint32_t>(out->data & 0xffffffffu);
}

StepResult ToyCpuExecutor::step(uint64_t maxInstructions, uint64_t maxCycles) {
    StepResult result;
    result.success = true;
    result.instructionsExecuted = 0;
    result.cyclesExecuted = 0;

    if (mDbg == nullptr) {
        fault(CpuErrorType::DeviceFault, mPc, 0);
        result.success = false;
        return result;
    }

    auto traceOption = (mDbg) ? mDbg->getTraceOptions() : TraceOptions{};
    bool hasBreakpoints = mDbg && mDbg->hasBreakpoints();

    while (result.instructionsExecuted < maxInstructions && result.cyclesExecuted < maxCycles) {
        bool logInstructions = traceOption.logInstruction;
        bool logMemEvents = traceOption.logMemEvents;
        bool logBranchPrediction = traceOption.logBranchPrediction;

        if (hasBreakpoints && mDbg->isBreakpoint(mPc)) {
             return result;
        }

        TraceRecord record;
        record.pc = mPc;
        record.cycleBegin = mCycle;
        record.isBranch = false;

        MemResponse fetch;
        uint32_t inst = fetchU32(mPc, &fetch);

        if (logMemEvents) {
            MemAccessEvent evt;
            evt.type = MemAccessType::Fetch;
            evt.address = mPc;
            evt.size = 4;
            evt.data = inst;
            evt.latencyCycles = fetch.latencyCycles;
            record.memEvents.push_back(evt);
        }

        if (!fetch.success) {
            mLastError = fetch.error;
            if (logMemEvents) {
                record.inst = 0;
                record.decoded = "FETCH_ERROR";
                record.cycleEnd = mCycle;
                mDbg->logTrace(record);
            }
            result.success = false;
            return result;
        }

        record.inst = inst;
        uint64_t pcBefore = mPc;
        mPc += 4;
        ++mCycle;
        
        result.instructionsExecuted++;
        result.cyclesExecuted++;
        
        uint8_t op = OpCode(inst);
        bool success = true;

        if (op == static_cast<uint8_t>(toy::Op::Nop)) {
            if (logInstructions) record.decoded = "NOP";
        } else if (op == static_cast<uint8_t>(toy::Op::Halt)) {
            if (logInstructions) record.decoded = "HALT";
            success = fault(CpuErrorType::None, pcBefore, 4);
        } else if (op == static_cast<uint8_t>(toy::Op::Lui)) {
            uint8_t rd = Rd(inst);
            uint16_t imm = Imm16(inst);
            if (logInstructions) record.decoded = "LUI r" + std::to_string(rd) + ", " + std::to_string(imm);
            
            uint64_t value = static_cast<uint64_t>(imm) << 16;
            setRegister(rd, value);
        } else if (op == static_cast<uint8_t>(toy::Op::Ori)) {
            uint8_t rd = Rd(inst);
            uint16_t imm = Imm16(inst);
            if (logInstructions) record.decoded = "ORI r" + std::to_string(rd) + ", " + std::to_string(imm);

            uint64_t value = getRegister(rd) | static_cast<uint64_t>(imm);
            setRegister(rd, value);
        } else if (op == static_cast<uint8_t>(toy::Op::Beq)) {
            uint8_t r0 = Rd(inst);
            uint8_t r1 = Rs(inst);
            int8_t off = Off8(inst);
            if (logInstructions) {
                record.decoded = "BEQ r" + std::to_string(r0) + ", r" + std::to_string(r1) +
                    ", " + std::to_string(off);
            }

            record.isBranch = true;
            record.branch.predictedTaken = false;
            record.branch.predictedTarget = mPc + OffsetToWords(off);

            bool taken = getRegister(r0) == getRegister(r1);
            record.branch.taken = taken;
            record.branch.target = mPc + OffsetToWords(off);
            if (taken) {
                mPc = mPc + OffsetToWords(off);
            }
        } else if (op == static_cast<uint8_t>(toy::Op::Lw)) {
            uint8_t rd = Rd(inst);
            uint8_t rs = Rs(inst);
            int8_t off = Off8(inst);
            if (logInstructions) {
                record.decoded = "LW r" + std::to_string(rd) + ", [r" + std::to_string(rs) + "+" + std::to_string(off) + "]";
            }

            uint64_t addr = getRegister(rs) + static_cast<int64_t>(off);
            MemAccess access;
            access.address = addr;
            access.size = 4;
            access.type = MemAccessType::Read;
            MemResponse r = mDbg->busRead(access);

            if (logMemEvents) {
                MemAccessEvent evt;
                evt.type = MemAccessType::Read;
                evt.address = addr;
                evt.size = 4;
                evt.data = r.data;
                evt.latencyCycles = r.latencyCycles;
                record.memEvents.push_back(evt);
            }

            if (!r.success) {
                mLastError = r.error;
                success = false;
            } else {
                setRegister(rd, static_cast<uint32_t>(r.data & 0xffffffffu));
            }
        } else if (op == static_cast<uint8_t>(toy::Op::Sw)) {
            uint8_t rs = Rd(inst);
            uint8_t rd = Rs(inst);
            int8_t off = Off8(inst);
            if (logInstructions) {
                record.decoded = "SW r" + std::to_string(rs) + ", [r" + std::to_string(rd) + "+" + std::to_string(off) + "]";
            }

            uint64_t addr = getRegister(rd) + static_cast<int64_t>(off);
            MemAccess access;
            access.address = addr;
            access.size = 4;
            access.type = MemAccessType::Write;
            access.data = static_cast<uint32_t>(getRegister(rs) & 0xffffffffu);
            MemResponse w = mDbg->busWrite(access);

            if (logMemEvents) {
                MemAccessEvent evt;
                evt.type = MemAccessType::Write;
                evt.address = addr;
                evt.size = 4;
                evt.data = access.data;
                evt.latencyCycles = w.latencyCycles;
                record.memEvents.push_back(evt);
            }

            if (!w.success) {
                mLastError = w.error;
                success = false;
            }
        } else {
            if (logInstructions) record.decoded = "INVALID_OP";
            success = fault(CpuErrorType::InvalidOp, pcBefore, 4);
        }

        record.cycleEnd = mCycle;
        if (logInstructions || logBranchPrediction || logMemEvents) mDbg->logTrace(record);

        if (!success) {
            result.success = false;
            return result;
        }
    }
    
    return result;
}

extern "C" ICpuExecutor* CreateCpuExecutor() {
    static ToyCpuExecutor cpu;
    return &cpu;
}
