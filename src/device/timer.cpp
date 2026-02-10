#include "emulator/device/timer.h"


namespace {
constexpr uint64_t kTimerLowOffset = 0x0;
constexpr uint64_t kTimerHighOffset = 0x4;
constexpr uint64_t kTimerCtrlOffset = 0x8;
constexpr uint32_t kTimerRegSize = 4;

bool isValidAccess(const MemAccess& access) {
    return access.size == kTimerRegSize;
}

MemResponse makeFault(const MemAccess& access) {
    MemResponse response;
    response.success = false;
    response.error.type = CpuErrorType::AccessFault;
    response.error.address = access.address;
    response.error.size = access.size;
    return response;
}

}

TimerDevice::TimerDevice() {
    setType(DeviceType::Timer);
    setReadHandler([this](const MemAccess& access) { return handleRead(access); });
    setWriteHandler([this](const MemAccess& access) { return handleWrite(access); });
    setTickHandler([this](uint64_t cycles) { handleTick(cycles); });
}

uint64_t TimerDevice::getCounterMicros() {
    return mAccumulatedMicros;
}

void TimerDevice::handleTick(uint64_t cycles) {
    mAccumulatedMicros += cycles;
}

MemResponse TimerDevice::handleRead(const MemAccess& access) {
    if (!isValidAccess(access)) {
        return makeFault(access);
    }
    if (access.address != kTimerLowOffset && access.address != kTimerHighOffset) {
        return makeFault(access);
    }
    uint64_t counter = getCounterMicros();
    MemResponse response;
    response.success = true;
    response.data = (access.address == kTimerLowOffset)
        ? static_cast<uint32_t>(counter & 0xffffffffu)
        : static_cast<uint32_t>((counter >> 32) & 0xffffffffu);
    return response;
}

MemResponse TimerDevice::handleWrite(const MemAccess& access) {
    if (!isValidAccess(access)) {
        return makeFault(access);
    }
    if (access.address == kTimerCtrlOffset) {
        mAccumulatedMicros = 0;
        MemResponse response;
        response.success = true;
        return response;
    }
    return makeFault(access);
}
