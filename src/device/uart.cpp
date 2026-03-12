#include "emulator/device/uart.h"
#include "emulator/log/logger.h"

namespace emulator {

Uart::Uart() {}

void Uart::reset() {
    mRxHead.store(0, std::memory_order_relaxed);
    mRxTail.store(0, std::memory_order_relaxed);
    mTxHead.store(0, std::memory_order_relaxed);
    mTxTail.store(0, std::memory_order_relaxed);
}

bool Uart::read(uint64_t offset, uint8_t* data, size_t len) {
    if (offset >= kUartSize) {
        for (size_t i = 0; i < len; ++i) {
            data[i] = 0;
        }
        return false;
    }
    
    if (offset == 0) {
        if (len >= 1) {
            uint32_t tail = mRxTail.load(std::memory_order_relaxed);
            uint32_t head = mRxHead.load(std::memory_order_acquire);
            
            if (head != tail) {
                data[0] = mRxBuffer[head & kFifoMask];
                mRxHead.store(head + 1, std::memory_order_release);
                DEBUG("UART: read RX=0x%02x ('%c')", data[0], 
                      data[0] >= 32 && data[0] < 127 ? data[0] : '.');
            } else {
                data[0] = 0;
            }
        }
        for (size_t i = 1; i < len; ++i) {
            data[i] = 0;
        }
    } else if (offset == 4) {
        uint8_t status = 0;
        uint32_t txTail = mTxTail.load(std::memory_order_relaxed);
        uint32_t txHead = mTxHead.load(std::memory_order_acquire);
        if ((txTail - txHead) < kUartFifoDepth) {
            status |= 0x01;
        }
        
        uint32_t rxHead = mRxHead.load(std::memory_order_relaxed);
        uint32_t rxTail = mRxTail.load(std::memory_order_acquire);
        if (rxTail != rxHead) {
            status |= 0x02;
        }
        data[0] = status;
        for (size_t i = 1; i < len; ++i) {
            data[i] = 0;
        }
        DEBUG("UART: read STATUS=0x%02x", status);
    } else {
        for (size_t i = 0; i < len; ++i) {
            data[i] = 0;
        }
    }
    return true;
}

bool Uart::write(uint64_t offset, const uint8_t* data, size_t len) {
    if (offset >= kUartSize) {
        return false;
    }
    
    if (offset == 0 && len >= 1) {
        uint32_t tail = mTxTail.load(std::memory_order_relaxed);
        uint32_t head = mTxHead.load(std::memory_order_acquire);
        
        if ((tail - head) < kUartFifoDepth) {
            mTxBuffer[tail & kFifoMask] = data[0];
            mTxTail.store(tail + 1, std::memory_order_release);
            DEBUG("UART: write TX=0x%02x ('%c')", data[0],
                  data[0] >= 32 && data[0] < 127 ? data[0] : '.');
        } else {
            WARN("UART: TX FIFO full, dropping byte 0x%02x", data[0]);
        }
    }
    return true;
}

void Uart::pushRx(uint8_t byte) {
    uint32_t tail = mRxTail.load(std::memory_order_relaxed);
    uint32_t head = mRxHead.load(std::memory_order_acquire);
    
    if ((tail - head) < kUartFifoDepth) {
        mRxBuffer[tail & kFifoMask] = byte;
        mRxTail.store(tail + 1, std::memory_order_release);
        DEBUG("UART: push RX=0x%02x ('%c')", byte,
              byte >= 32 && byte < 127 ? byte : '.');
    } else {
        WARN("UART: RX FIFO full, dropping byte 0x%02x", byte);
    }
}

bool Uart::popTx(uint8_t* byte) {
    uint32_t head = mTxHead.load(std::memory_order_relaxed);
    uint32_t tail = mTxTail.load(std::memory_order_acquire);
    
    if (head == tail) {
        return false;
    }
    *byte = mTxBuffer[head & kFifoMask];
    mTxHead.store(head + 1, std::memory_order_release);
    return true;
}

bool Uart::hasTxData() const {
    uint32_t head = mTxHead.load(std::memory_order_acquire);
    uint32_t tail = mTxTail.load(std::memory_order_acquire);
    return head != tail;
}

bool Uart::hasRxData() const {
    uint32_t head = mRxHead.load(std::memory_order_acquire);
    uint32_t tail = mRxTail.load(std::memory_order_acquire);
    return head != tail;
}

} // namespace emulator