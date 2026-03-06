#ifndef TEST_TOY_CPU_EXECUTOR_H
#define TEST_TOY_CPU_EXECUTOR_H

#include <cstdint>
#include <vector>

#include "emulator/cpu/cpu.h"

class ToyCpuExecutor;

ToyCpuExecutor *GetLastToyCpu();

class Debugger;

class ToyCpuExecutor : public ICpuExecutor {
public:
  explicit ToyCpuExecutor(std::shared_ptr<IBus> bus);
  ~ToyCpuExecutor() override;

  void reset() override;
  void cycle(CommitState &state) override;

  // Let's check cpu.h again properly.
  // virtual void getRegState(RegState& state) const = 0;
  // virtual uint32_t getRegisterCount() const = 0;
  // virtual void getCsrState(CsrState& state) const = 0;
  // virtual uint32_t getCsrCount() const = 0;
  // virtual void setResetPc(uint64_t pc) = 0;

  void getRegState(RegState &state) const override;
  uint32_t getRegisterCount() const override;
  void getCsrState(CsrState &state) const override;
  uint32_t getCsrCount() const override;
  void setResetPc(uint64_t pc) override;

  // Helper methods for tests (not overrides)
  uint64_t getPc() const;
  void setPc(uint64_t pc);
  uint64_t getCycle() const;
  uint64_t getRegister(uint32_t regId) const;
  void setRegister(uint32_t regId, uint64_t value);
  CpuErrorType getLastError() const;

  // setDebugger removed from ICpuExecutor.
  // ToyCpuExecutor needs to access Bus directly via getBus().

private:
  bool fault(CpuErrorType type, uint64_t addr, uint32_t size,
             CommitInfo &commit);
  uint32_t fetchU32(uint64_t pc, MemResponse *out, CommitInfo &commit);

  static constexpr uint32_t kRegCount = 16;
  uint64_t mRegs[kRegCount] = {};
  uint64_t mPc = 0;
  uint64_t mCycle = 0;

  CpuErrorType mLastFault = CpuErrorType::None;
};

#endif
