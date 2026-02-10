#ifndef EMULATOR_DEVICE_TIMER_H
#define EMULATOR_DEVICE_TIMER_H

#include "emulator/device/device.h"

class TimerDevice : public Device {
public:
    TimerDevice();

    uint64_t getCounterMicros();

private:
    uint64_t mAccumulatedMicros = 0;
    
    void handleTick(uint64_t cycles);
    MemResponse handleRead(const MemAccess& access);
    MemResponse handleWrite(const MemAccess& access);
};

#endif
