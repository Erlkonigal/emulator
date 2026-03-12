#include "toy_cpu_executor.h"

#include "emulator/device/ram.h"
#include "emulator/device/rom.h"
#include "emulator/device/uart.h"
#include "emulator/generated/hardware_config.h"

#include <cstring>
#include <iostream>

#include "toy_isa.h"

namespace emulator {

namespace {

using toy::kRamBase;
using toy::kUartBase;

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

void ToyCpuExecutor::setResetPc(uint64_t pc) { mResetPc = static_cast<uint32_t>(pc); }

uint32_t ToyCpuExecutor::getPc() const { return mPc; }

void ToyCpuExecutor::setPc(uint32_t pc) { mPc = pc; }

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

uint32_t ToyCpuExecutor::getRegister(uint32_t regId) const {
  if (regId >= kRegCount || regId == 0) {
    return 0;
  }
  return mRegs[regId];
}

void ToyCpuExecutor::setRegister(uint32_t regId, uint32_t value) {
  if (regId >= kRegCount || regId == 0) {
    return;
  }
  mRegs[regId] = value;
}

CpuErrorType ToyCpuExecutor::getLastError() const { return mLastFault; }

uint8_t ToyCpuExecutor::readByte(uint32_t addr) const {
  uint8_t b = 0;
  
  if (addr >= kUartBase && addr < kUartBase + kUartSize) {
    Uart::getInstance().read(addr - kUartBase, &b, 1);
  } else if (addr >= kRamBase) {
    Ram::getInstance().read(addr - kRamBase, &b, 1);
  } else {
    Rom::getInstance().read(addr, &b, 1);
  }
  
  return b;
}

uint16_t ToyCpuExecutor::readHalf(uint32_t addr) const {
  if (addr >= kUartBase && addr < kUartBase + kUartSize) {
    return 0;
  }
  
  uint16_t val = 0;
  uint8_t bytes[2] = {0, 0};
  
  if (addr >= kRamBase) {
    Ram::getInstance().read(addr - kRamBase, bytes, 2);
  } else {
    Rom::getInstance().read(addr, bytes, 2);
  }
  
  val = static_cast<uint16_t>(bytes[0]) | (static_cast<uint16_t>(bytes[1]) << 8);
  return val;
}

void ToyCpuExecutor::writeByte(uint32_t addr, uint8_t data) {
  if (addr >= kUartBase && addr < kUartBase + kUartSize) {
    Uart::getInstance().write(addr - kUartBase, &data, 1);
  } else if (addr >= kRamBase) {
    Ram::getInstance().write(addr - kRamBase, &data, 1);
  }
}

void ToyCpuExecutor::writeHalf(uint32_t addr, uint16_t data) {
  if (addr >= kUartBase && addr < kUartBase + kUartSize) {
    return;
  }
  
  uint8_t bytes[2];
  bytes[0] = data & 0xff;
  bytes[1] = (data >> 8) & 0xff;
  
  if (addr >= kRamBase) {
    Ram::getInstance().write(addr - kRamBase, bytes, 2);
  }
}

void ToyCpuExecutor::writeMem(uint32_t addr, uint32_t data) {
  if (addr >= kUartBase && addr < kUartBase + kUartSize) {
    return;
  }
  
  uint8_t bytes[4];
  bytes[0] = data & 0xff;
  bytes[1] = (data >> 8) & 0xff;
  bytes[2] = (data >> 16) & 0xff;
  bytes[3] = (data >> 24) & 0xff;
  
  if (addr >= kRamBase) {
    Ram::getInstance().write(addr - kRamBase, bytes, 4);
  }
}

uint32_t ToyCpuExecutor::readMem(uint32_t addr) const {
  if (addr >= kUartBase && addr < kUartBase + kUartSize) {
    return 0;
  }
  
  uint8_t bytes[4] = {0, 0, 0, 0};
  
  if (addr >= kRamBase) {
    Ram::getInstance().read(addr - kRamBase, bytes, 4);
  } else {
    Rom::getInstance().read(addr, bytes, 4);
  }
  
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

  uint32_t pcBefore = mPc;
  commit.pc = mPc;
  commit.isRegWrite = false;
  commit.isRamWrite = false;
  commit.isUncached = false;
  commit.isCsrAccess = false;
  commit.errorType = CpuErrorType::None;
  commit.regId = 0;
  commit.regData = 0;
  commit.ramOffset = 0;
  commit.ramData = 0;

  uint32_t inst = readMem(mPc);
  commit.inst = inst;
  mPc += 4;
  mCycle++;

  uint8_t op = OpCode(inst);
  commit.valid = true;

  switch (op) {
  case static_cast<uint8_t>(toy::Op::Nop):
    break;

  case static_cast<uint8_t>(toy::Op::Halt):
    commit.errorType = CpuErrorType::Halt;
    commit.errorMsg = "Halt instruction";
    mLastFault = CpuErrorType::Halt;
    break;

  case static_cast<uint8_t>(toy::Op::Lui): {
    uint8_t rd = Rd(inst);
    uint16_t imm = Imm16(inst);
    uint32_t value = static_cast<uint32_t>(imm) << 16;
    commit.isRegWrite = true;
    commit.regId = rd;
    commit.regData = value;
    setRegister(rd, value);
    break;
  }

  case static_cast<uint8_t>(toy::Op::Ori): {
    uint8_t rd = Rd(inst);
    uint16_t imm = Imm16(inst);
    uint32_t value = getRegister(rd) | imm;
    commit.isRegWrite = true;
    commit.regId = rd;
    commit.regData = value;
    setRegister(rd, value);
    break;
  }

  case static_cast<uint8_t>(toy::Op::Andi): {
    uint8_t rd = Rd(inst);
    uint16_t imm = Imm16(inst);
    uint32_t value = getRegister(rd) & imm;
    commit.isRegWrite = true;
    commit.regId = rd;
    commit.regData = value;
    setRegister(rd, value);
    break;
  }

  case static_cast<uint8_t>(toy::Op::Beq): {
    uint8_t r0 = Rd(inst);
    uint8_t r1 = Rs(inst);
    int8_t off = Off8(inst);
    bool taken = getRegister(r0) == getRegister(r1);
    if (taken) {
      mPc = pcBefore + OffsetToWords(off);
    }
    break;
  }

  case static_cast<uint8_t>(toy::Op::Lw): {
    uint8_t rd = Rd(inst);
    uint8_t rs = Rs(inst);
    int8_t off = Off8(inst);
    uint32_t offset = getRegister(rs) + static_cast<int32_t>(off);
    uint32_t value = readMem(offset);
    commit.isRegWrite = true;
    commit.regId = rd;
    commit.regData = value;
    setRegister(rd, value);
    break;
  }

  case static_cast<uint8_t>(toy::Op::Sw): {
    uint8_t rd = Rd(inst);
    uint8_t rs = Rs(inst);
    int8_t off = Off8(inst);
    uint32_t offset = getRegister(rs) + static_cast<int32_t>(off);
    uint32_t data = getRegister(rd);
    commit.isRamWrite = true;
    commit.ramOffset = offset;
    commit.ramData = data;
    writeMem(offset, data);
    break;
  }

  case static_cast<uint8_t>(toy::Op::Add): {
    uint8_t rd = Rd(inst);
    uint8_t rs = Rs(inst);
    uint8_t rt = Off8(inst);
    uint32_t value = getRegister(rs) + getRegister(rt);
    commit.isRegWrite = true;
    commit.regId = rd;
    commit.regData = value;
    setRegister(rd, value);
    break;
  }

  case static_cast<uint8_t>(toy::Op::Sub): {
    uint8_t rd = Rd(inst);
    uint8_t rs = Rs(inst);
    uint8_t rt = Off8(inst);
    uint32_t value = getRegister(rs) - getRegister(rt);
    commit.isRegWrite = true;
    commit.regId = rd;
    commit.regData = value;
    setRegister(rd, value);
    break;
  }

  case static_cast<uint8_t>(toy::Op::Lb): {
    uint8_t rd = Rd(inst);
    uint8_t rs = Rs(inst);
    int8_t off = Off8(inst);
    uint32_t addr = getRegister(rs) + static_cast<int32_t>(off);
    uint8_t val = readByte(addr);
    uint32_t value = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(val)));
    commit.isRegWrite = true;
    commit.regId = rd;
    commit.regData = value;
    setRegister(rd, value);
    break;
  }

  case static_cast<uint8_t>(toy::Op::Lh): {
    uint8_t rd = Rd(inst);
    uint8_t rs = Rs(inst);
    int8_t off = Off8(inst);
    uint32_t addr = getRegister(rs) + static_cast<int32_t>(off);
    uint16_t val = readHalf(addr);
    uint32_t value = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(val)));
    commit.isRegWrite = true;
    commit.regId = rd;
    commit.regData = value;
    setRegister(rd, value);
    break;
  }

  case static_cast<uint8_t>(toy::Op::Lbu): {
    uint8_t rd = Rd(inst);
    uint8_t rs = Rs(inst);
    int8_t off = Off8(inst);
    uint32_t addr = getRegister(rs) + static_cast<int32_t>(off);
    uint8_t value = readByte(addr);
    commit.isRegWrite = true;
    commit.regId = rd;
    commit.regData = value;
    setRegister(rd, value);
    break;
  }

  case static_cast<uint8_t>(toy::Op::Lhu): {
    uint8_t rd = Rd(inst);
    uint8_t rs = Rs(inst);
    int8_t off = Off8(inst);
    uint32_t addr = getRegister(rs) + static_cast<int32_t>(off);
    uint16_t value = readHalf(addr);
    commit.isRegWrite = true;
    commit.regId = rd;
    commit.regData = value;
    setRegister(rd, value);
    break;
  }

  case static_cast<uint8_t>(toy::Op::Sb): {
    uint8_t rd = Rd(inst);
    uint8_t rs = Rs(inst);
    int8_t off = Off8(inst);
    uint32_t addr = getRegister(rs) + static_cast<int32_t>(off);
    uint8_t data = static_cast<uint8_t>(getRegister(rd));
    commit.isRamWrite = true;
    commit.ramOffset = addr;
    commit.ramData = data;
    writeByte(addr, data);
    break;
  }

  case static_cast<uint8_t>(toy::Op::Sh): {
    uint8_t rd = Rd(inst);
    uint8_t rs = Rs(inst);
    int8_t off = Off8(inst);
    uint32_t addr = getRegister(rs) + static_cast<int32_t>(off);
    uint16_t data = static_cast<uint16_t>(getRegister(rd));
    commit.isRamWrite = true;
    commit.ramOffset = addr;
    commit.ramData = data;
    writeHalf(addr, data);
    break;
  }

  case static_cast<uint8_t>(toy::Op::Srli): {
    uint8_t rd = Rd(inst);
    uint8_t rs = Rs(inst);
    uint8_t shamt = Off8(inst) & 0x1f;
    uint32_t value = getRegister(rs) >> shamt;
    commit.isRegWrite = true;
    commit.regId = rd;
    commit.regData = value;
    setRegister(rd, value);
    break;
  }

  case static_cast<uint8_t>(toy::Op::Slli): {
    uint8_t rd = Rd(inst);
    uint8_t rs = Rs(inst);
    uint8_t shamt = Off8(inst) & 0x1f;
    uint32_t value = getRegister(rs) << shamt;
    commit.isRegWrite = true;
    commit.regId = rd;
    commit.regData = value;
    setRegister(rd, value);
    break;
  }

  case static_cast<uint8_t>(toy::Op::And): {
    uint8_t rd = Rd(inst);
    uint8_t rs = Rs(inst);
    uint8_t rt = Off8(inst);
    uint32_t value = getRegister(rs) & getRegister(rt);
    commit.isRegWrite = true;
    commit.regId = rd;
    commit.regData = value;
    setRegister(rd, value);
    break;
  }

  default:
    commit.errorType = CpuErrorType::Stop;
    commit.errorMsg = "Unknown opcode";
    mLastFault = CpuErrorType::Stop;
    break;
  }
}

} // namespace emulator