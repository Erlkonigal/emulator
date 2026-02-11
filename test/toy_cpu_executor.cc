#include "toy_cpu_executor.h"

#include <cstring>
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

bool ToyCpuExecutor::fault(CpuErrorType type, uint64_t addr, uint32_t size, CycleResult& result) {
    ErrorInfo err;
    err.type = type;
    err.message = "Fault at address 0x" + std::to_string(addr);
    result.errors.push_back(err);
    return false;
}

uint32_t ToyCpuExecutor::fetchU32(uint64_t pc, MemResponse* out, CycleResult& result) {
    if (out == nullptr || mDbg == nullptr) {
        return 0;
    }
    MemAccess access;
    access.address = pc;
    access.size = 4;
    access.type = MemAccessType::Fetch;
    *out = mDbg->busRead(access);
    if (!out->success) {
        ErrorInfo err;
        err.type = CpuErrorType::AccessFault;
        err.pc = pc;
        err.message = "Fetch failed";
        result.errors.push_back(err);
        return 0;
    }
    return static_cast<uint32_t>(out->data & 0xffffffffu);
}

void ToyCpuExecutor::cycle(CycleResult& result) {
    result.commits.clear();
    result.errors.clear();

    if (mDbg == nullptr) {
        ErrorInfo err;
        err.type = CpuErrorType::DeviceFault;
        err.message = "No debugger attached";
        result.errors.push_back(err);
        return;
    }

    CommitInfo commit;
    commit.pc = mPc;
    commit.isBranch = false;

    MemResponse fetch;
    uint32_t inst = fetchU32(mPc, &fetch, result);

    if (!fetch.success) {
        ErrorInfo err;
        err.type = CpuErrorType::AccessFault;
        err.pc = mPc;
        err.message = "Fetch failed";
        result.errors.push_back(err);
        mCycle++;
        return;
    }

    commit.inst = inst;
    uint64_t pcBefore = mPc;
    mPc += 4;
    mCycle++;

    uint8_t op = OpCode(inst);
    bool success = true;

    if (op == static_cast<uint8_t>(toy::Op::Nop)) {
        commit.decoded = "NOP";
    } else if (op == static_cast<uint8_t>(toy::Op::Halt)) {
        commit.decoded = "HALT";
        success = fault(CpuErrorType::None, pcBefore, 4, result);
    } else if (op == static_cast<uint8_t>(toy::Op::Lui)) {
        uint8_t rd = Rd(inst);
        uint16_t imm = Imm16(inst);
        commit.decoded = "LUI r" + std::to_string(rd) + ", " + std::to_string(imm);

        uint64_t value = static_cast<uint64_t>(imm) << 16;
        RegisterEvent regEvt;
        regEvt.regId = rd;
        regEvt.newValue = value;
        commit.regEvents.push_back(regEvt);
        setRegister(rd, value);
    } else if (op == static_cast<uint8_t>(toy::Op::Ori)) {
        uint8_t rd = Rd(inst);
        uint16_t imm = Imm16(inst);
        commit.decoded = "ORI r" + std::to_string(rd) + ", " + std::to_string(imm);

        uint64_t value = getRegister(rd) | static_cast<uint64_t>(imm);
        RegisterEvent regEvt;
        regEvt.regId = rd;
        regEvt.newValue = value;
        commit.regEvents.push_back(regEvt);
        setRegister(rd, value);
    } else if (op == static_cast<uint8_t>(toy::Op::Beq)) {
        uint8_t r0 = Rd(inst);
        uint8_t r1 = Rs(inst);
        int8_t off = Off8(inst);
        commit.decoded = "BEQ r" + std::to_string(r0) + ", r" + std::to_string(r1) +
            ", " + std::to_string(off);

        commit.isBranch = true;
        commit.branch.predictedTaken = false;
        commit.branch.predictedTarget = mPc + OffsetToWords(off);

        bool taken = getRegister(r0) == getRegister(r1);
        commit.branch.taken = taken;
        commit.branch.target = mPc + OffsetToWords(off);
        if (taken) {
            mPc = mPc + OffsetToWords(off);
        }
    } else if (op == static_cast<uint8_t>(toy::Op::Lw)) {
        uint8_t rd = Rd(inst);
        uint8_t rs = Rs(inst);
        int8_t off = Off8(inst);
        commit.decoded = "LW r" + std::to_string(rd) + ", [r" + std::to_string(rs) + "+" + std::to_string(off) + "]";

        uint64_t addr = getRegister(rs) + static_cast<int64_t>(off);
        MemAccess access;
        access.address = addr;
        access.size = 4;
        access.type = MemAccessType::Read;
        MemResponse r = mDbg->busRead(access);

        if (!r.success) {
            ErrorInfo err;
            err.type = r.errorType;
            err.pc = pcBefore;
            err.message = "Load failed";
            result.errors.push_back(err);
            success = false;
        } else {
            uint32_t value = static_cast<uint32_t>(r.data & 0xffffffffu);
            RegisterEvent regEvt;
            regEvt.regId = rd;
            regEvt.newValue = value;
            commit.regEvents.push_back(regEvt);
            setRegister(rd, value);
        }
    } else if (op == static_cast<uint8_t>(toy::Op::Sw)) {
        uint8_t rd = Rd(inst);
        uint8_t rs = Rs(inst);
        int8_t off = Off8(inst);
        commit.decoded = "SW r" + std::to_string(rd) + ", [r" + std::to_string(rs) + "+" + std::to_string(off) + "]";

        uint64_t addr = getRegister(rs) + static_cast<int64_t>(off);
        MemAccess access;
        access.address = addr;
        access.size = 4;
        access.type = MemAccessType::Write;
        access.data = static_cast<uint32_t>(getRegister(rd) & 0xffffffffu);
        MemResponse w = mDbg->busWrite(access);

        MemAccessEvent memEvt;
        memEvt.type = MemAccessType::Write;
        memEvt.address = addr;
        memEvt.size = 4;
        memEvt.data = access.data;
        commit.memEvents.push_back(memEvt);

        if (!w.success) {
            ErrorInfo err;
            err.type = w.errorType;
            err.pc = pcBefore;
            err.message = "Store failed";
            result.errors.push_back(err);
            success = false;
        }
    } else {
        commit.decoded = "INVALID_OP";
        success = fault(CpuErrorType::InvalidOp, pcBefore, 4, result);
    }

    if (success || !result.errors.empty()) {
        result.commits.push_back(commit);
    }
}

extern "C" ICpuExecutor* CreateCpuExecutor() {
    static ToyCpuExecutor cpu;
    return &cpu;
}
