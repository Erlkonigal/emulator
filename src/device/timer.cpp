#include "emulator/device/timer.h"


namespace {
constexpr uint64_t kTimerLowOffset = 0x0;
constexpr uint64_t kTimerHighOffset = 0x4;
constexpr uint64_t kTimerCtrlOffset = 0x8;
constexpr uint32_t kTimerRegSize = 4;
// kTickBatchCycles no longer used as we don't batch system calls

bool IsValidAccess(const MemAccess& access) {
    return access.Size == kTimerRegSize;
}

MemResponse MakeFault(const MemAccess& access) {
    MemResponse response;
    response.Success = false;
    response.Error.Type = CpuErrorType::AccessFault;
    response.Error.Address = access.Address;
    response.Error.Size = access.Size;
    return response;
}

}

TimerDevice::TimerDevice() {
    SetType(DeviceType::Timer);
    SetReadHandler([this](const MemAccess& access) { return HandleRead(access); });
    SetWriteHandler([this](const MemAccess& access) { return HandleWrite(access); });
    SetTickHandler([this](uint64_t cycles) { HandleTick(cycles); });
}

uint64_t TimerDevice::GetCounterMicros() {
    return AccumulatedMicros;
}

void TimerDevice::HandleTick(uint64_t cycles) {
    // Assume 1MHz, so 1 cycle = 1 microsecond.
    AccumulatedMicros += cycles;
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
    response.Success = true;
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
        MemResponse response;
        response.Success = true;
        return response;
    }
    return MakeFault(access);
}
