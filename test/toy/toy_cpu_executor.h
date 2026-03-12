#ifndef TEST_TOY_CPU_EXECUTOR_H
#define TEST_TOY_CPU_EXECUTOR_H

#include <cstdint>

#include "emulator/cpu/cpu.h"

namespace emulator {

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

  uint32_t getPc() const;
  void setPc(uint32_t pc);
  uint64_t getCycle() const;
  uint32_t getRegister(uint32_t regId) const;
  void setRegister(uint32_t regId, uint32_t value);
  CpuErrorType getLastError() const;

  void writeMem(uint32_t addr, uint32_t data);
  uint32_t readMem(uint32_t addr) const;
  
  uint8_t readByte(uint32_t addr) const;
  uint16_t readHalf(uint32_t addr) const;
  void writeByte(uint32_t addr, uint8_t data);
  void writeHalf(uint32_t addr, uint16_t data);

private:
  static constexpr uint32_t kRegCount = 16;
  static constexpr uint32_t kCsrCount = 64;

  uint32_t mRegs[kRegCount] = {};
  uint64_t mCsrs[kCsrCount] = {};
  uint32_t mPc = 0;
  uint64_t mCycle = 0;
  uint32_t mResetPc = 0;

  CpuErrorType mLastFault = CpuErrorType::None;
};

ToyCpuExecutor *GetLastToyCpu();

} // namespace emulator

#endif