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

    void PushRx(uint8_t ch);
    void SetTxHandler(TxHandler handler);
    void Flush();

private:
    std::deque<uint8_t> RxBuffer;
    std::string TxBuffer;
    mutable std::mutex Mutex;
    TxHandler TxCallback;
    uint64_t IdleCycles = 0;

    uint32_t GetStatus() const;
    void FlushTxLocked();
    void HandleTick(uint64_t cycles);
    MemResponse HandleRead(const MemAccess& access);
    MemResponse HandleWrite(const MemAccess& access);
};

#endif
