#include "emulator/device/memory.h"

#include <fstream>

namespace {
bool IsAccessValid(const std::vector<uint8_t>& storage, const MemAccess& access) {
    if (access.Size == 0 || access.Size > sizeof(uint64_t)) {
        return false;
    }
    uint64_t size = static_cast<uint64_t>(storage.size());
    if (access.Address >= size) {
        return false;
    }
    if (access.Size > size - access.Address) {
        return false;
    }
    return true;
}

MemResponse MakeFault(const MemAccess& access) {
    MemResponse response;
    response.Success = false;
    response.Error.Type = CpuErrorType::AccessFault;
    response.Error.Address = access.Address;
    response.Error.Size = access.Size;
    return response;
}

uint64_t ReadValue(const std::vector<uint8_t>& storage, const MemAccess& access) {
    uint64_t value = 0;
    for (uint32_t i = 0; i < access.Size; ++i) {
        value |= static_cast<uint64_t>(storage[static_cast<size_t>(access.Address + i)])
            << (8 * i);
    }
    return value;
}

void WriteValue(std::vector<uint8_t>* storage, const MemAccess& access) {
    if (storage == nullptr) {
        return;
    }
    uint64_t value = access.Data;
    for (uint32_t i = 0; i < access.Size; ++i) {
        (*storage)[static_cast<size_t>(access.Address + i)] =
            static_cast<uint8_t>(value & 0xff);
        value >>= 8;
    }
}

} // namespace

MemoryDevice::MemoryDevice(uint64_t size, bool readOnly)
    : Storage(size, 0), ReadOnly(readOnly) {
    SetType(readOnly ? DeviceType::Rom : DeviceType::Ram);
    SetReadHandler([this](const MemAccess& access) { return HandleRead(access); });
    SetWriteHandler([this](const MemAccess& access) { return HandleWrite(access); });
}

bool MemoryDevice::LoadImage(const std::string& path, uint64_t offset) {
    if (offset >= Storage.size()) {
        return false;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }
    input.read(reinterpret_cast<char*>(Storage.data() + offset),
        static_cast<std::streamsize>(Storage.size() - offset));
    return input.good() || input.eof();
}

uint64_t MemoryDevice::GetSize() const {
    return Storage.size();
}

bool MemoryDevice::IsReadOnly() const {
    return ReadOnly;
}

MemResponse MemoryDevice::HandleRead(const MemAccess& access) {
    if (!::IsAccessValid(Storage, access)) {
        return MakeFault(access);
    }
    MemResponse response;
    response.Success = true;
    response.Data = ReadValue(Storage, access);
    return response;
}

MemResponse MemoryDevice::HandleWrite(const MemAccess& access) {
    if (!::IsAccessValid(Storage, access)) {
        return MakeFault(access);
    }
    if (ReadOnly) {
        return MakeFault(access);
    }
    WriteValue(&Storage, access);
    MemResponse response;
    response.Success = true;
    return response;
}
