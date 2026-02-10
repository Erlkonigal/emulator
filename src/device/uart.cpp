#include "emulator/device/uart.h"
#include "emulator/logging/logger.h"

#include <cstdio>
#include <string>
#include <utility>

namespace {
constexpr uint64_t kUartDataOffset = 0x0;
constexpr uint64_t kUartStatusOffset = 0x4;
constexpr uint32_t kUartStatusRxReady = 1u << 0;
constexpr uint32_t kUartStatusTxReady = 1u << 1;
constexpr uint32_t kUartRegSize = 4;
constexpr size_t kUartFlushThreshold = 256;
constexpr uint32_t kUartFlushIdleThreshold = 10000;

bool isValidAccess(const MemAccess& access) {
    return access.size == kUartRegSize;
}

uint8_t extractByte(uint64_t value) {
    return static_cast<uint8_t>(value & 0xff);
}

MemResponse makeFault(uint64_t address, uint32_t size) {
    MemResponse response;
    response.success = false;
    response.error.type = CpuErrorType::AccessFault;
    response.error.address = address;
    response.error.size = size;
    return response;
}
}

UartDevice::UartDevice() {
    setType(DeviceType::Uart);
    setReadHandler([this](const MemAccess& access) { return handleRead(access); });
    setWriteHandler([this](const MemAccess& access) { return handleWrite(access); });
    setTickHandler([this](uint64_t cycles) { handleTick(cycles); });
}

UartDevice::~UartDevice() {
    std::lock_guard<std::mutex> lock(mMutex);
    flushTxLocked();
}

void UartDevice::pushRx(uint8_t ch) {
    std::lock_guard<std::mutex> lock(mMutex);
    mRxBuffer.push_back(ch);
}

void UartDevice::flush() {
    std::lock_guard<std::mutex> lock(mMutex);
    flushTxLocked();
}

uint32_t UartDevice::getStatus() const {
    std::lock_guard<std::mutex> lock(mMutex);
    uint32_t status = kUartStatusTxReady;
    if (!mRxBuffer.empty()) {
        status |= kUartStatusRxReady;
    }
    return status;
}

MemResponse UartDevice::handleRead(const MemAccess& access) {
    if (!isValidAccess(access)) {
        return makeFault(access.address, access.size);
    }
    if (access.address == kUartStatusOffset) {
        MemResponse response;
        response.success = true;
        response.data = getStatus();
        return response;
    }
    if (access.address == kUartDataOffset) {
        MemResponse response;
        response.success = true;
        std::lock_guard<std::mutex> lock(mMutex);
        if (mRxBuffer.empty()) {
            response.data = 0;
        } else {
            response.data = mRxBuffer.front();
            mRxBuffer.pop_front();
        }
        return response;
    }
    return makeFault(access.address, access.size);
}

MemResponse UartDevice::handleWrite(const MemAccess& access) {
    if (!isValidAccess(access)) {
        return makeFault(access.address, access.size);
    }
    if (access.address == kUartDataOffset) {
        uint8_t ch = extractByte(access.data);
        std::lock_guard<std::mutex> lock(mMutex);
        mTxBuffer.push_back(static_cast<char>(ch));
        mIdleCycles = 0;
        if (mTxBuffer.size() >= kUartFlushThreshold) {
            flushTxLocked();
        }
        MemResponse response;
        response.success = true;
        return response;
    }
    return makeFault(access.address, access.size);
}

void UartDevice::handleTick(uint64_t cycles) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mTxBuffer.empty()) {
        mIdleCycles = 0;
        return;
    }
    mIdleCycles += cycles;
    if (mIdleCycles >= kUartFlushIdleThreshold) {
        flushTxLocked();
        mIdleCycles = 0;
    }
}

void UartDevice::flushTxLocked() {
    if (mTxBuffer.empty()) {
        return;
    }
    logging::device("%s", mTxBuffer.c_str());
    mTxBuffer.clear();
}
