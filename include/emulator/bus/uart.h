#pragma once

#include <cstdint>
#include <cstddef>
#include <mutex>
#include <queue>

#include "emulator/bus/device.h"
#include "emulator/generated/hardware_config.h"
#include "emulator/utils/singleton.h"

constexpr size_t kUartFifoDepth = 16;

class Uart : public IDevice, public Singleton<Uart> {
public:
    Uart(uint64_t baseAddr = kUartBase);
    ~Uart() override = default;

    Uart(const Uart&) = delete;
    Uart& operator=(const Uart&) = delete;

    bool read(uint64_t offset, uint8_t* data, size_t len) override;
    bool write(uint64_t offset, const uint8_t* data, size_t len) override;

    uint64_t getBaseAddr() const override { return mBaseAddr; }
    uint64_t getSize() const override { return kUartSize; }
    const char* getName() const override { return "UART"; }

    void pushRx(uint8_t byte);
    bool popTx(uint8_t* byte);
    bool hasTxData() const;
    bool hasRxData() const;

    void setBaseAddr(uint64_t baseAddr) { mBaseAddr = baseAddr; }

private:
    uint64_t mBaseAddr;
    mutable std::mutex mMutex;
    std::queue<uint8_t> mRxFifo;
    std::queue<uint8_t> mTxFifo;

    friend class Singleton<Uart>;
};