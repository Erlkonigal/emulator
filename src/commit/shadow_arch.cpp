#include "emulator/commit/shadow_arch.h"
#include "emulator/log/logger.h"

void ShadowArch::update(const CommitInfo &commit) {
  if (!commit.valid) {
    WARN("Invalid commit, maybe triggered by assert");
    return;
  }

  pc = commit.pc;

  if (commit.isRegWrite && commit.regId < kMaxNumRegisters) {
    regs[commit.regId] = commit.regData;
  }

  if (commit.isMemWrite && commit.memAddress < kRamSize) {
    mem[commit.memAddress] = commit.memData;
  }

  if (commit.isCsrAccess && commit.csrState) {
    for (size_t i = 0; i < kMaxNumCsr; i++) {
      csrs[i] = (*commit.csrState)[i];
    }
  }
}