#include "emulator/device.h"

Device::Device() = default;

MemResponse Device::Read(const MemAccess& access) {
    if (ReadCallback) {
        return ReadCallback(access);
    }
    MemResponse response;
    response.Result = CpuResult::Error;
    response.Error.Type = CpuErrorType::DeviceFault;
    response.Error.Address = access.Address;
    response.Error.Size = access.Size;
    return response;
}

MemResponse Device::Write(const MemAccess& access) {
    if (WriteCallback) {
        return WriteCallback(access);
    }
    MemResponse response;
    response.Result = CpuResult::Error;
    response.Error.Type = CpuErrorType::DeviceFault;
    response.Error.Address = access.Address;
    response.Error.Size = access.Size;
    return response;
}

void Device::Tick(uint32_t cycles) {
    if (TickCallback) {
        TickCallback(cycles);
    }
}

DeviceType Device::GetType() const {
    return Type;
}

void Device::SetReadHandler(ReadHandler handler) {
    ReadCallback = std::move(handler);
}

void Device::SetWriteHandler(WriteHandler handler) {
    WriteCallback = std::move(handler);
}

void Device::SetTickHandler(TickHandler handler) {
    TickCallback = std::move(handler);
}

void Device::SetType(DeviceType type) {
    Type = type;
}
