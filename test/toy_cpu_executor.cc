#include "toy_cpu_executor.h"

#include <chrono>
#include <cstring>
#include <thread>

#include "emulator/bus.h"

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

void ToyCpuExecutor::SetMemoryBus(MemoryBus* bus) {
    Bus = bus;
}

void ToyCpuExecutor::SetTraceSink(TraceSink* traceSink) {
    Trace = traceSink;
}

uint32_t ToyCpuExecutor::GetRegisterCount() const {
    return kRegCount;
}

CpuResult ToyCpuExecutor::Fault(CpuErrorType type, uint64_t addr, uint32_t size) {
    LastError.Type = type;
    LastError.Address = addr;
    LastError.Size = size;
    return CpuResult::Error;
}

uint32_t ToyCpuExecutor::FetchU32(uint64_t pc, MemResponse* out) {
    if (out == nullptr || Bus == nullptr) {
        return 0;
    }
    MemAccess access;
    access.Address = pc;
    access.Size = 4;
    access.Type = MemAccessType::Fetch;
    *out = Bus->Read(access);
    if (out->Result != CpuResult::Success) {
        return 0;
    }
    return static_cast<uint32_t>(out->Data & 0xffffffffu);
}

CpuResult ToyCpuExecutor::StepInstruction() {
    if (Bus == nullptr) {
        return Fault(CpuErrorType::DeviceFault, Pc, 0);
    }

    MemResponse fetch;
    uint32_t inst = FetchU32(Pc, &fetch);
    if (fetch.Result != CpuResult::Success) {
        LastError = fetch.Error;
        return CpuResult::Error;
    }

    uint64_t pcBefore = Pc;
    Pc += 4;
    ++Cycle;

    uint8_t op = OpCode(inst);
    if (op == static_cast<uint8_t>(toy::Op::Nop)) {
        return CpuResult::Success;
    }
    if (op == static_cast<uint8_t>(toy::Op::Halt)) {
        return Fault(CpuErrorType::Halt, pcBefore, 4);
    }
    if (op == static_cast<uint8_t>(toy::Op::Lui)) {
        uint8_t rd = Rd(inst);
        uint64_t value = static_cast<uint64_t>(Imm16(inst)) << 16;
        SetRegister(rd, value);
        return CpuResult::Success;
    }
    if (op == static_cast<uint8_t>(toy::Op::Ori)) {
        uint8_t rd = Rd(inst);
        uint64_t value = GetRegister(rd) | static_cast<uint64_t>(Imm16(inst));
        SetRegister(rd, value);
        return CpuResult::Success;
    }
    if (op == static_cast<uint8_t>(toy::Op::SleepMs)) {
        uint16_t ms = Imm16(inst);
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        return CpuResult::Success;
    }
    if (op == static_cast<uint8_t>(toy::Op::Lw)) {
        uint8_t rd = Rd(inst);
        uint8_t rs = Rs(inst);
        uint64_t addr = GetRegister(rs) + static_cast<int64_t>(Off8(inst));
        MemAccess access;
        access.Address = addr;
        access.Size = 4;
        access.Type = MemAccessType::Read;
        MemResponse r = Bus->Read(access);
        if (r.Result != CpuResult::Success) {
            LastError = r.Error;
            return CpuResult::Error;
        }
        SetRegister(rd, static_cast<uint32_t>(r.Data & 0xffffffffu));
        return CpuResult::Success;
    }
    if (op == static_cast<uint8_t>(toy::Op::Sw)) {
        uint8_t rs = Rd(inst);
        uint8_t rd = Rs(inst);
        uint64_t addr = GetRegister(rd) + static_cast<int64_t>(Off8(inst));
        MemAccess access;
        access.Address = addr;
        access.Size = 4;
        access.Type = MemAccessType::Write;
        access.Data = static_cast<uint32_t>(GetRegister(rs) & 0xffffffffu);
        MemResponse w = Bus->Write(access);
        if (w.Result != CpuResult::Success) {
            LastError = w.Error;
            return CpuResult::Error;
        }
        return CpuResult::Success;
    }

    return Fault(CpuErrorType::InvalidOp, pcBefore, 4);
}

extern "C" ICpuExecutor* CreateCpuExecutor() {
    static ToyCpuExecutor cpu;
    return &cpu;
}
