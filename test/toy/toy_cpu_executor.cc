#include "toy_cpu_executor.h"

#include "emulator/bus/bus.h"

#include <cstring>
#include <iostream>

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

ToyCpuExecutor::ToyCpuExecutor() {
  g_last = this;
  reset();
}

ToyCpuExecutor::~ToyCpuExecutor() = default;

void ToyCpuExecutor::reset() {
  std::memset(mRegs, 0, sizeof(mRegs));
  std::memset(mCsrs, 0, sizeof(mCsrs));
  mPc = mResetPc;
  mCycle = 0;
  mLastFault = CpuErrorType::None;
}

void ToyCpuExecutor::setResetPc(uint64_t pc) { mResetPc = pc; }

uint64_t ToyCpuExecutor::getPc() const { return mPc; }

void ToyCpuExecutor::setPc(uint64_t pc) { mPc = pc; }

uint64_t ToyCpuExecutor::getCycle() const { return mCycle; }

void ToyCpuExecutor::getRegState(RegState &state) const {
  for (size_t i = 0; i < kRegCount && i < kMaxNumRegisters; ++i) {
    state[i] = mRegs[i];
  }
}

size_t ToyCpuExecutor::getRegCount() const { return kRegCount; }

void ToyCpuExecutor::getCsrState(CsrState &state) const {
  for (size_t i = 0; i < kCsrCount && i < kMaxNumCsr; ++i) {
    state[i] = mCsrs[i];
  }
}

size_t ToyCpuExecutor::getCsrCount() const { return kCsrCount; }

uint64_t ToyCpuExecutor::getRegister(uint32_t regId) const {
  if (regId >= kRegCount || regId == 0) {
    return 0;
  }
  return mRegs[regId];
}

void ToyCpuExecutor::setRegister(uint32_t regId, uint64_t value) {
  if (regId >= kRegCount || regId == 0) {
    return;
  }
  mRegs[regId] = value;
}

CpuErrorType ToyCpuExecutor::getLastError() const { return mLastFault; }

void ToyCpuExecutor::writeMem(uint64_t addr, uint32_t data) {
  auto& bus = Bus::getInstance();
  uint8_t bytes[4];
  bytes[0] = data & 0xff;
  bytes[1] = (data >> 8) & 0xff;
  bytes[2] = (data >> 16) & 0xff;
  bytes[3] = (data >> 24) & 0xff;
  bus.write(addr, bytes, 4);
}

uint32_t ToyCpuExecutor::readMem(uint64_t addr) const {
  auto& bus = Bus::getInstance();
  uint8_t bytes[4] = {0, 0, 0, 0};
  bus.read(addr, bytes, 4);
  return static_cast<uint32_t>(bytes[0]) |
         (static_cast<uint32_t>(bytes[1]) << 8) |
         (static_cast<uint32_t>(bytes[2]) << 16) |
         (static_cast<uint32_t>(bytes[3]) << 24);
}

void ToyCpuExecutor::cycle(CommitArray &commits) {
  CommitInfo &commit = commits[0];
  commit.valid = false;

  for (size_t i = 1; i < kMaxNumCommitsPerCycle; ++i) {
    commits[i].valid = false;
  }

  uint64_t pcBefore = mPc;
  commit.pc = mPc;
  commit.isRegWrite = false;
  commit.isMemWrite = false;
  commit.isCsrAccess = false;
  commit.errorType = CpuErrorType::None;
  commit.regId = 0;
  commit.regData = 0;
  commit.memAddress = 0;
  commit.memData = 0;

  uint32_t inst = readMem(mPc);
  commit.inst = inst;
  mPc += 4;
  mCycle++;

  uint8_t op = OpCode(inst);
  commit.valid = true;

  if (op == static_cast<uint8_t>(toy::Op::Nop)) {
  } else if (op == static_cast<uint8_t>(toy::Op::Halt)) {
    commit.errorType = CpuErrorType::Halt;
    commit.errorMsg = "Halt instruction";
    mLastFault = CpuErrorType::Halt;
  } else if (op == static_cast<uint8_t>(toy::Op::Lui)) {
    uint8_t rd = Rd(inst);
    uint16_t imm = Imm16(inst);
    uint64_t value = static_cast<uint64_t>(imm) << 16;

    commit.isRegWrite = true;
    commit.regId = rd;
    commit.regData = static_cast<uint32_t>(value);
    setRegister(rd, value);
  } else if (op == static_cast<uint8_t>(toy::Op::Ori)) {
    uint8_t rd = Rd(inst);
    uint16_t imm = Imm16(inst);
    uint64_t value = getRegister(rd) | static_cast<uint64_t>(imm);

    commit.isRegWrite = true;
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

    uint32_t value = readMem(addr);

    commit.isRegWrite = true;
    commit.regId = rd;
    commit.regData = value;

    setRegister(rd, value);
  } else if (op == static_cast<uint8_t>(toy::Op::Sw)) {
    uint8_t rd = Rd(inst);
    uint8_t rs = Rs(inst);
    int8_t off = Off8(inst);
    uint64_t addr = getRegister(rs) + static_cast<int64_t>(off);
    uint32_t data = static_cast<uint32_t>(getRegister(rd) & 0xffffffffu);

    commit.isMemWrite = true;
    commit.memAddress = addr;
    commit.memData = data;

    writeMem(addr, data);
  } else if (op == static_cast<uint8_t>(toy::Op::Add)) {
    uint8_t rd = Rd(inst);
    uint8_t rs = Rs(inst);
    uint8_t rt = Off8(inst);
    uint64_t value = getRegister(rs) + getRegister(rt);

    commit.isRegWrite = true;
    commit.regId = rd;
    commit.regData = static_cast<uint32_t>(value);
    setRegister(rd, value);
  } else if (op == static_cast<uint8_t>(toy::Op::Sub)) {
    uint8_t rd = Rd(inst);
    uint8_t rs = Rs(inst);
    uint8_t rt = Off8(inst);
    uint64_t value = getRegister(rs) - getRegister(rt);

    commit.isRegWrite = true;
    commit.regId = rd;
    commit.regData = static_cast<uint32_t>(value);
    setRegister(rd, value);
  } else {
    commit.errorType = CpuErrorType::Stop;
    commit.errorMsg = "Unknown opcode";
    mLastFault = CpuErrorType::Stop;
  }
}