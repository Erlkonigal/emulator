#pragma once

#include <cstddef>
#include <cstdint>

constexpr size_t kCommitQueueSize = 1024;
constexpr uint32_t kCpuFrequency = 1000000;
constexpr uint16_t kDefaultDebugPort = 1234;
constexpr size_t kMaxInstDecodeLen = 64;
constexpr uint32_t kMaxNumCommitsPerConsumption = 256;
constexpr uint32_t kMaxNumCommitsPerCycle = 8;
constexpr uint32_t kMaxNumCsr = 64;
constexpr uint32_t kMaxNumRegisters = 32;
constexpr uint64_t kPadding = 64ull;
constexpr uint64_t kRamBase = 0x80000000ull;
constexpr size_t kRamSize = 0x10000000;
constexpr uint64_t kRomBase = 0x00000000ull;
constexpr uint64_t kUartBase = 0x10000000ull;
constexpr size_t kUartSize = 0x1000;

constexpr const char *kRomDeviceName = "ROM";
constexpr const char *kRamDeviceName = "RAM";
