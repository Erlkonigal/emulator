#ifndef EMULATOR_DEVICE_TIMER_H
#define EMULATOR_DEVICE_TIMER_H

#include "emulator/device/device.h"

class TimerDevice : public Device {
public:
    TimerDevice();

    uint64_t GetCounterMicros();

private:
    uint64_t AccumulatedMicros = 0;
    
    void HandleTick(uint64_t cycles);
    MemResponse HandleRead(const MemAccess& access);
    MemResponse HandleWrite(const MemAccess& access);
};

#endif
