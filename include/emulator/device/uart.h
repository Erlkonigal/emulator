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
    using TxHandler = std::function<void(const std::string&)>;

    UartDevice();
    ~UartDevice();

    void pushRx(uint8_t ch);
    void setTxHandler(TxHandler handler);
    void flush();

private:
    std::deque<uint8_t> mRxBuffer;
    std::string mTxBuffer;
    mutable std::mutex mMutex;
    TxHandler mTxCallback;
    uint64_t mIdleCycles = 0;

    uint32_t getStatus() const;
    void flushTxLocked();
    void handleTick(uint64_t cycles);
    MemResponse handleRead(const MemAccess& access);
    MemResponse handleWrite(const MemAccess& access);
};

#endif
