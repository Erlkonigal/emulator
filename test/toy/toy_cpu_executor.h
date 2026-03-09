#ifndef TEST_TOY_CPU_EXECUTOR_H
#define TEST_TOY_CPU_EXECUTOR_H

#include <cstdint>

#include "emulator/cpu/cpu.h"

class ToyCpuExecutor;

ToyCpuExecutor *GetLastToyCpu();

class ToyCpuExecutor : public ICpuExecutor {
public:
  ToyCpuExecutor();
  ~ToyCpuExecutor() override;

  void reset() override;
  void cycle(CommitArray &commits) override;

  void getRegState(RegState &state) const override;
  size_t getRegCount() const override;
  void getCsrState(CsrState &state) const override;
  size_t getCsrCount() const override;
  void setResetPc(uint64_t pc) override;

  uint64_t getPc() const;
  void setPc(uint64_t pc);
  uint64_t getCycle() const;
  uint64_t getRegister(uint32_t regId) const;
  void setRegister(uint32_t regId, uint64_t value);
  CpuErrorType getLastError() const;

  void writeMem(uint64_t addr, uint32_t data);
  uint32_t readMem(uint64_t addr) const;

private:
  static constexpr uint32_t kRegCount = 16;
  static constexpr uint32_t kCsrCount = 64;

  uint64_t mRegs[kRegCount] = {};
  uint64_t mCsrs[kCsrCount] = {};
  uint64_t mPc = 0;
  uint64_t mCycle = 0;
  uint64_t mResetPc = 0;

  CpuErrorType mLastFault = CpuErrorType::None;
};

#endif