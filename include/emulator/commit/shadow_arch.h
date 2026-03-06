#pragma once

#include <cstdint>

#include "emulator/cpu/cpu.h"
#include "emulator/utils/config.h"
#include "emulator/utils/singleton.h"

class ShadowArch : public Singleton<ShadowArch> {
public:
  void update(const CommitInfo &commit);

  uint64_t readReg(uint32_t regId) const {
    if (regId < kMaxNumRegisters)
      return regs[regId];
    else
      return 0;
  }
  uint64_t readCsr(uint32_t csrId) const {
    if (csrId < kMaxNumCsr)
      return csrs[csrId];
    else
      return 0;
  }
  uint8_t readMem(uint64_t offset) const {
    if (offset < kRamSize)
      return mem[offset];
    else
      return 0;
  }
  uint64_t readPc() const { return pc; }

private:
  uint64_t pc = 0;
  RegState regs = {};
  CsrState csrs = {};
  uint8_t mem[kRamSize] = {};
  friend class Singleton<ShadowArch>;
};