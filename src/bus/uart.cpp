#include "emulator/bus/uart.h"
#include "emulator/log/logger.h"

Uart::Uart(uint64_t baseAddr) : mBaseAddr(baseAddr) {}

bool Uart::read(uint64_t offset, uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> lock(mMutex);
    
    if (offset == 0) {
        if (len >= 1) {
            if (!mRxFifo.empty()) {
                data[0] = mRxFifo.front();
                mRxFifo.pop();
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
        if (mTxFifo.size() < kUartFifoDepth) {
            status |= 0x01;
        }
        if (!mRxFifo.empty()) {
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
    std::lock_guard<std::mutex> lock(mMutex);
    
    if (offset == 0 && len >= 1) {
        if (mTxFifo.size() < kUartFifoDepth) {
            mTxFifo.push(data[0]);
            DEBUG("UART: write TX=0x%02x ('%c')", data[0],
                  data[0] >= 32 && data[0] < 127 ? data[0] : '.');
        } else {
            WARN("UART: TX FIFO full, dropping byte 0x%02x", data[0]);
        }
    }
    return true;
}

void Uart::pushRx(uint8_t byte) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mRxFifo.size() < kUartFifoDepth) {
        mRxFifo.push(byte);
        DEBUG("UART: push RX=0x%02x ('%c')", byte,
              byte >= 32 && byte < 127 ? byte : '.');
    } else {
        WARN("UART: RX FIFO full, dropping byte 0x%02x", byte);
    }
}

bool Uart::popTx(uint8_t* byte) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mTxFifo.empty()) {
        return false;
    }
    *byte = mTxFifo.front();
    mTxFifo.pop();
    return true;
}

bool Uart::hasTxData() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return !mTxFifo.empty();
}

bool Uart::hasRxData() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return !mRxFifo.empty();
}