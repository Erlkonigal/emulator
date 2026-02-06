#include "emulator/device.h"

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

bool IsValidAccess(const MemAccess& access) {
    return access.Size == kUartRegSize;
}

uint8_t ExtractByte(uint64_t value) {
    return static_cast<uint8_t>(value & 0xff);
}

MemResponse MakeFault(uint64_t address, uint32_t size) {
    MemResponse response;
    response.Result = CpuResult::Error;
    response.Error.Type = CpuErrorType::AccessFault;
    response.Error.Address = address;
    response.Error.Size = size;
    return response;
}
}

UartDevice::UartDevice() {
    SetType(DeviceType::Uart);
    SetReadHandler([this](const MemAccess& access) { return HandleRead(access); });
    SetWriteHandler([this](const MemAccess& access) { return HandleWrite(access); });
}

UartDevice::~UartDevice() {
    std::lock_guard<std::mutex> lock(Mutex);
    FlushTxLocked();
}

void UartDevice::PushRx(uint8_t ch) {
    std::lock_guard<std::mutex> lock(Mutex);
    RxBuffer.push_back(ch);
}

void UartDevice::SetTxHandler(TxHandler handler) {
    std::lock_guard<std::mutex> lock(Mutex);
    TxCallback = std::move(handler);
}

uint32_t UartDevice::GetStatus() const {
    std::lock_guard<std::mutex> lock(Mutex);
    uint32_t status = kUartStatusTxReady;
    if (!RxBuffer.empty()) {
        status |= kUartStatusRxReady;
    }
    return status;
}

MemResponse UartDevice::HandleRead(const MemAccess& access) {
    if (!IsValidAccess(access)) {
        return MakeFault(access.Address, access.Size);
    }
    if (access.Address == kUartStatusOffset) {
        MemResponse response;
        response.Result = CpuResult::Success;
        response.Data = GetStatus();
        return response;
    }
    if (access.Address == kUartDataOffset) {
        MemResponse response;
        response.Result = CpuResult::Success;
        std::lock_guard<std::mutex> lock(Mutex);
        if (RxBuffer.empty()) {
            response.Data = 0;
        } else {
            response.Data = RxBuffer.front();
            RxBuffer.pop_front();
        }
        return response;
    }
    return MakeFault(access.Address, access.Size);
}

MemResponse UartDevice::HandleWrite(const MemAccess& access) {
    if (!IsValidAccess(access)) {
        return MakeFault(access.Address, access.Size);
    }
    if (access.Address == kUartDataOffset) {
        uint8_t ch = ExtractByte(access.Data);
        std::lock_guard<std::mutex> lock(Mutex);
        TxBuffer.push_back(static_cast<char>(ch));
        if (TxBuffer.size() >= kUartFlushThreshold) {
            FlushTxLocked();
        }
        MemResponse response;
        response.Result = CpuResult::Success;
        return response;
    }
    return MakeFault(access.Address, access.Size);
}

void UartDevice::FlushTxLocked() {
    if (TxBuffer.empty()) {
        return;
    }
    if (TxCallback) {
        TxCallback(TxBuffer);
    } else {
        std::fwrite(TxBuffer.data(), 1, TxBuffer.size(), stdout);
        std::fflush(stdout);
    }
    TxBuffer.clear();
}
