#include "emulator/device.h"


namespace {
constexpr uint64_t kTimerLowOffset = 0x0;
constexpr uint64_t kTimerHighOffset = 0x4;
constexpr uint64_t kTimerCtrlOffset = 0x8;
constexpr uint32_t kTimerRegSize = 4;
constexpr uint32_t kTickBatchCycles = 1000;

bool IsValidAccess(const MemAccess& access) {
    return access.Size == kTimerRegSize;
}

MemResponse MakeFault(const MemAccess& access) {
    MemResponse response;
    response.Result = CpuResult::Error;
    response.Error.Type = CpuErrorType::AccessFault;
    response.Error.Address = access.Address;
    response.Error.Size = access.Size;
    return response;
}

}

TimerDevice::TimerDevice() {
    SetType(DeviceType::Timer);
    LastTick = std::chrono::steady_clock::now();
    SetReadHandler([this](const MemAccess& access) { return HandleRead(access); });
    SetWriteHandler([this](const MemAccess& access) { return HandleWrite(access); });
    SetTickHandler([this](uint32_t cycles) { HandleTick(cycles); });
}

uint64_t TimerDevice::GetCounterMicros() {
    auto now = std::chrono::steady_clock::now();
    auto delta = std::chrono::duration_cast<std::chrono::microseconds>(now - LastTick).count();
    LastTick = now;
    if (delta > 0) {
        AccumulatedMicros += static_cast<uint64_t>(delta);
    }
    return AccumulatedMicros;
}

void TimerDevice::AccumulateCycles(uint32_t cycles) {
    uint32_t total = PendingCycles + cycles;
    PendingCycles = total % kTickBatchCycles;
    if (total >= kTickBatchCycles) {
        GetCounterMicros();
    }
}

void TimerDevice::HandleTick(uint32_t cycles) {
    AccumulateCycles(cycles);
}

MemResponse TimerDevice::HandleRead(const MemAccess& access) {
    if (!IsValidAccess(access)) {
        return MakeFault(access);
    }
    if (access.Address != kTimerLowOffset && access.Address != kTimerHighOffset) {
        return MakeFault(access);
    }
    uint64_t counter = GetCounterMicros();
    MemResponse response;
    response.Result = CpuResult::Success;
    response.Data = (access.Address == kTimerLowOffset)
        ? static_cast<uint32_t>(counter & 0xffffffffu)
        : static_cast<uint32_t>((counter >> 32) & 0xffffffffu);
    return response;
}

MemResponse TimerDevice::HandleWrite(const MemAccess& access) {
    if (!IsValidAccess(access)) {
        return MakeFault(access);
    }
    if (access.Address == kTimerCtrlOffset) {
        AccumulatedMicros = 0;
        LastTick = std::chrono::steady_clock::now();
        PendingCycles = 0;
        MemResponse response;
        response.Result = CpuResult::Success;
        return response;
    }
    return MakeFault(access);
}
