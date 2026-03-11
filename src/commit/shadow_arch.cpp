#include "emulator/commit/shadow_arch.h"
#include "emulator/log/logger.h"

#include <sys/mman.h>
#include <cstring>

void ShadowArch::init() {
  if (mem != nullptr) {
    munmap(mem, kRamSize);
  }
  mem = static_cast<uint8_t*>(mmap(
      nullptr, kRamSize, PROT_READ | PROT_WRITE,
      MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0));
  if (mem == MAP_FAILED) {
    mem = nullptr;
  }
}

ShadowArch::~ShadowArch() {
  if (mem != nullptr) {
    munmap(mem, kRamSize);
    mem = nullptr;
  }
}

void ShadowArch::reset() {
  pc = 0;
  for (size_t i = 0; i < kMaxNumRegisters; ++i) {
    regs[i] = 0;
  }
  for (size_t i = 0; i < kMaxNumCsr; ++i) {
    csrs[i] = 0;
  }
  if (mem != nullptr) {
    madvise(mem, kRamSize, MADV_DONTNEED);
  }
}

void ShadowArch::update(const CommitInfo &commit) {
  if (!commit.valid) {
    WARN("Invalid commit, maybe triggered by assert");
    return;
  }

  pc = commit.pc;

  if (commit.isRegWrite && commit.regId < kMaxNumRegisters) {
    regs[commit.regId] = commit.regData;
  }

  if (commit.isRamWrite && commit.ramOffset < kRamSize) {
    mem[commit.ramOffset] = commit.ramData;
  }

  if (commit.isCsrAccess && commit.csrState) {
    for (size_t i = 0; i < kMaxNumCsr; i++) {
      csrs[i] = (*commit.csrState)[i];
    }
  }
}