#include "toy_cpu_executor.h"

#include <cstring>
#include <iostream>
#include <vector>

#include "toy_isa.h"

namespace {

ToyCpuExecutor *g_last = nullptr;

uint8_t OpCode(uint32_t inst) {
  return static_cast<uint8_t>((inst >> 24) & 0xff);
}

uint8_t Rd(uint32_t inst) { return static_cast<uint8_t>((inst >> 16) & 0xff); }

uint8_t Rs(uint32_t inst) { return static_cast<uint8_t>((inst >> 8) & 0xff); }

uint16_t Imm16(uint32_t inst) { return static_cast<uint16_t>(inst & 0xffffu); }

int8_t Off8(uint32_t inst) { return static_cast<int8_t>(inst & 0xffu); }

int64_t OffsetToWords(int8_t off) { return static_cast<int64_t>(off) * 4; }

} // namespace

ToyCpuExecutor *GetLastToyCpu() { return g_last; }

ToyCpuExecutor::ToyCpuExecutor(std::shared_ptr<IBus> bus) : ICpuExecutor(bus) {
  g_last = this;
  reset();
}

ToyCpuExecutor::~ToyCpuExecutor() = default;

void ToyCpuExecutor::reset() {
  std::memset(mRegs, 0, sizeof(mRegs));
  mPc = 0;
  mCycle = 0;
}

void ToyCpuExecutor::setResetPc(uint64_t pc) { mPc = pc; }

uint64_t ToyCpuExecutor::getPc() const { return mPc; }

void ToyCpuExecutor::setPc(uint64_t pc) { mPc = pc; }

uint64_t ToyCpuExecutor::getCycle() const { return mCycle; }

void ToyCpuExecutor::getRegState(RegState &state) const {
  for (size_t i = 0; i < kRegCount && i < kMaxNumRegisters; ++i) {
    state.regs[i] = mRegs[i];
  }
}

uint32_t ToyCpuExecutor::getRegisterCount() const { return kRegCount; }

void ToyCpuExecutor::getCsrState(CsrState &state) const {
  (void)state;
  // No CSRs in toy CPU
}

uint32_t ToyCpuExecutor::getCsrCount() const { return 0; }

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

CpuErrorType ToyCpuExecutor::getLastError() const { return mLastFault; }

bool ToyCpuExecutor::fault(CpuErrorType type, uint64_t addr, uint32_t size,
                           CommitInfo &commit) {
  (void)addr; // Unused in simplistic fault reporting here
  (void)size;
  commit.errorType = type;
  commit.errorMsg = "Fault occurred"; // Simplified
  return false;
}

uint32_t ToyCpuExecutor::fetchU32(uint64_t pc, MemResponse *out,
                                  CommitInfo &commit) {
  if (out == nullptr) {
    return 0;
  }
  MemAccess access;
  access.address = pc;
  access.size = 4;
  access.type = MemAccessType::Read; // Fetch is read
  *out = getBus()->read(access);

  if (out->error != MemErrorType::None) {
    commit.errorType = CpuErrorType::Stop;
    commit.errorMsg = "Fetch failed";
    return 0;
  }
  return static_cast<uint32_t>(out->data & 0xffffffffu);
}

void ToyCpuExecutor::cycle(CommitState &state) {
  // We only produce 1 commit per cycle for toy cpu
  CommitInfo &commit = state.commits[0];
  commit.valid = false;

  // Clear other commits
  for (size_t i = 1; i < kMaxNumCommitsPerCycle; ++i) {
    state.commits[i].valid = false;
  }

  uint64_t pcBefore = mPc;
  commit.pc = mPc;
  // Initialize other fields
  commit.regWrite = false;
  commit.memWrite = false;
  commit.hasCsrAccess = false;
  commit.errorType = CpuErrorType::None;

  MemResponse fetch;
  uint32_t inst = fetchU32(mPc, &fetch, commit);

  if (fetch.error != MemErrorType::None) {
    commit.valid = true;
    mCycle++;
    return;
  }

  commit.inst = inst;
  mPc += 4;
  mCycle++;

  uint8_t op = OpCode(inst);
  // Default valid
  commit.valid = true;

  if (op == static_cast<uint8_t>(toy::Op::Nop)) {
    // Nothing
  } else if (op == static_cast<uint8_t>(toy::Op::Halt)) {
    fault(CpuErrorType::Halt, pcBefore, 4, commit);
  } else if (op == static_cast<uint8_t>(toy::Op::Lui)) {
    uint8_t rd = Rd(inst);
    uint16_t imm = Imm16(inst);
    uint64_t value = static_cast<uint64_t>(imm) << 16;

    commit.regWrite = true;
    commit.regId = rd;
    commit.regData = static_cast<uint32_t>(value);
    setRegister(rd, value);
  } else if (op == static_cast<uint8_t>(toy::Op::Ori)) {
    uint8_t rd = Rd(inst);
    uint16_t imm = Imm16(inst);
    uint64_t value = getRegister(rd) | static_cast<uint64_t>(imm);

    commit.regWrite = true;
    commit.regId = rd;
    commit.regData = static_cast<uint32_t>(value);
    setRegister(rd, value);
  } else if (op == static_cast<uint8_t>(toy::Op::Beq)) {
    uint8_t r0 = Rd(inst);
    uint8_t r1 = Rs(inst);
    int8_t off = Off8(inst);

    bool taken = getRegister(r0) == getRegister(r1);
    if (taken) {
      mPc = pcBefore + OffsetToWords(off);
    }
  } else if (op == static_cast<uint8_t>(toy::Op::Lw)) {
    uint8_t rd = Rd(inst);
    uint8_t rs = Rs(inst);
    int8_t off = Off8(inst);
    uint64_t addr = getRegister(rs) + static_cast<int64_t>(off);

    MemAccess access;
    access.address = addr;
    access.size = 4;
    access.type = MemAccessType::Read;
    MemResponse r = getBus()->read(access);

    if (r.error != MemErrorType::None) {
      commit.errorType = CpuErrorType::Stop;
      commit.errorMsg = "Load failed";
      mLastFault = CpuErrorType::Stop;
    } else {
      uint32_t value = static_cast<uint32_t>(r.data & 0xffffffffu);

      commit.regWrite = true;
      commit.regId = rd;
      commit.regData = value;

      setRegister(rd, value);
    }
  } else if (op == static_cast<uint8_t>(toy::Op::Sw)) {
    uint8_t rd = Rd(inst);
    uint8_t rs = Rs(inst);
    int8_t off = Off8(inst);
    uint64_t addr = getRegister(rs) + static_cast<int64_t>(off);

    MemAccess access;
    access.address = addr;
    access.size = 4;
    access.type = MemAccessType::Write;
    access.data = static_cast<uint32_t>(getRegister(rd) & 0xffffffffu);
    MemResponse w = getBus()->write(access);

    commit.memWrite = true;
    commit.memAddress = addr;
    commit.memData = access.data;

    if (w.error != MemErrorType::None) {
      commit.errorType = CpuErrorType::Stop;
      commit.errorMsg = "Store failed";
      mLastFault = CpuErrorType::Stop;
    }
  } else {
    fault(CpuErrorType::Stop, pcBefore, 4, commit);
  }
}

std::shared_ptr<ICpuExecutor> createCpuExecutor(std::shared_ptr<IBus> bus) {
  return std::make_shared<ToyCpuExecutor>(bus);
}
