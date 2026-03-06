#include "emulator/commit/shadow_arch.h"
#include "emulator/log/logger.h"

void ShadowArch::update(const CommitInfo &commit) {
  if (!commit.valid) {
    WARN("Invalid commit, maybe triggered by assert");
    goto _exit;
  }

  pc = commit.pc;

  if (commit.isRegWrite) {
    regs[commit.regId] = commit.regData;
  }

  if (commit.isMemWrite) {
    if (Bus::getInstance().contains(commit.memAddress, kRamDeviceName)) {
      mem[commit.memAddress - kRamBase] = commit.memData;
    }
  }

  if (commit.isCsrAccess) {
    for (size_t i = 0; i < std::extent<CsrState>::value; i++) {
      csrs[i] = (*commit.csrState)[i];
    }
  }

_exit:
  return;
}