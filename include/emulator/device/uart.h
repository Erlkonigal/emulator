#ifndef EMULATOR_DEVICE_UART_H
#define EMULATOR_DEVICE_UART_H

#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <vector>
#include "emulator/device/device.h"

class UartDevice : public Device {
public:

    UartDevice();
    ~UartDevice();

    void pushRx(uint8_t ch);
    void flush();

private:
    std::deque<uint8_t> mRxBuffer;
    std::string mTxBuffer;
    mutable std::mutex mMutex;
    uint64_t mIdleCycles = 0;

    uint32_t getStatus() const;
    void flushTxLocked();
    void handleTick(uint64_t cycles);
    MemResponse handleRead(const MemAccess& access);
    MemResponse handleWrite(const MemAccess& access);
};

#endif
