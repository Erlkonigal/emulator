#pragma once

#include <cstdint>
#include <memory>

#include "emulator/generated/hardware_config.h"

enum class CpuErrorType {
  None,
  Stop,
  Halt,
  Assert,
};

using RegState = uint64_t[kMaxNumRegisters];
using CsrState = uint64_t[kMaxNumCsr];

struct alignas(kPadding) CommitInfo {
  bool valid;
  // fetch
  uint64_t pc;
  uint32_t inst;
  char decode[kMaxInstDecodeLen];
  // register access
  bool isRegWrite;
  uint32_t regId;
  uint32_t regData;
  // mem access
  bool isMemWrite;
  bool isMemUncached;
  uint64_t memAddress;
  uint32_t memData;
  // csr ctrl
  // including csr instructions, trap and interrupt
  // if there is csr access, we will allocate a CsrState and put it here
  bool isCsrAccess;
  CsrState *csrState;
  // error handling
  CpuErrorType errorType;
  const char *errorMsg;
  // packet instruction support
  bool isPacketHead;
  bool isPacketTail;
};

using CommitArray = CommitInfo[kMaxNumCommitsPerCycle];

class ICpuExecutor {
public:
  ICpuExecutor() = default;
  virtual ~ICpuExecutor() = default;

  ICpuExecutor(const ICpuExecutor &) = delete;
  ICpuExecutor &operator=(const ICpuExecutor &) = delete;

  virtual void reset() = 0;
  virtual void cycle(CommitArray &commits) = 0;

  virtual void setResetPc(uint64_t pc) = 0;

  virtual void getRegState(RegState &state) const = 0;
  virtual void getCsrState(CsrState &state) const = 0;

  virtual size_t getRegCount() const = 0;
  virtual size_t getCsrCount() const = 0;
};