#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <array>

#include "emulator/device/device.h"
#include "emulator/generated/hardware_config.h"
#include "emulator/utils/singleton.h"

namespace emulator {

constexpr size_t kUartFifoDepth = 1024;

class Uart : public Singleton<Uart>, public IDevice {
public:
    Uart();
    ~Uart() = default;

    bool read(uint64_t offset, uint8_t* data, size_t len) override;
    bool write(uint64_t offset, const uint8_t* data, size_t len) override;

    void reset() override;
    void pushRx(uint8_t byte);
    bool popTx(uint8_t* byte);
    bool hasTxData() const;
    bool hasRxData() const;

private:
    static constexpr size_t kFifoMask = kUartFifoDepth - 1;
    static_assert((kUartFifoDepth & kFifoMask) == 0, "kUartFifoDepth must be power of 2");
    
    alignas(64) std::atomic<uint32_t> mRxHead{0};
    alignas(64) std::atomic<uint32_t> mRxTail{0};
    std::array<uint8_t, kUartFifoDepth> mRxBuffer{};
    
    alignas(64) std::atomic<uint32_t> mTxHead{0};
    alignas(64) std::atomic<uint32_t> mTxTail{0};
    std::array<uint8_t, kUartFifoDepth> mTxBuffer{};
};

} // namespace emulator