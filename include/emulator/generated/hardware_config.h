#pragma once

#include <cstddef>
#include <cstdint>

constexpr uint64_t kRamSize = 0x10000000ull;
constexpr uint64_t kUartSize = 0x1000ull;
constexpr uint32_t kDefaultDebugPort = 1234;
constexpr uint32_t kCpuFrequency = 1000000;
constexpr uint32_t kMaxNumRegisters = 32;
constexpr uint32_t kMaxNumCsr = 64;
constexpr uint32_t kMaxInstDecodeLen = 64;
constexpr uint32_t kCommitQueueSize = 1024;
constexpr uint32_t kMaxNumCommitsPerCycle = 8;
constexpr uint32_t kMaxNumCommitsPerConsumption = 256;
constexpr uint32_t kPadding = 64;
constexpr bool kEnableTrace = false;
