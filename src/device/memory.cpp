#include "emulator/device/memory.h"

#include <fstream>

namespace {
bool isAccessValid(const std::vector<uint8_t>& storage, const MemAccess& access) {
    if (access.size == 0 || access.size > sizeof(uint64_t)) {
        return false;
    }
    uint64_t size = static_cast<uint64_t>(storage.size());
    if (access.address >= size) {
        return false;
    }
    if (access.size > size - access.address) {
        return false;
    }
    return true;
}

MemResponse makeFault(const MemAccess& access) {
    MemResponse response;
    response.success = false;
    response.error.type = CpuErrorType::AccessFault;
    response.error.address = access.address;
    response.error.size = access.size;
    return response;
}

uint64_t readValue(const std::vector<uint8_t>& storage, const MemAccess& access) {
    uint64_t value = 0;
    for (uint32_t i = 0; i < access.size; ++i) {
        value |= static_cast<uint64_t>(storage[static_cast<size_t>(access.address + i)])
            << (8 * i);
    }
    return value;
}

void writeValue(std::vector<uint8_t>* storage, const MemAccess& access) {
    if (storage == nullptr) {
        return;
    }
    uint64_t value = access.data;
    for (uint32_t i = 0; i < access.size; ++i) {
        (*storage)[static_cast<size_t>(access.address + i)] =
            static_cast<uint8_t>(value & 0xff);
        value >>= 8;
    }
}

} // namespace

MemoryDevice::MemoryDevice(uint64_t size, bool readOnly)
    : mStorage(size, 0), mReadOnly(readOnly) {
    setType(readOnly ? DeviceType::Rom : DeviceType::Ram);
    setReadHandler([this](const MemAccess& access) { return handleRead(access); });
    setWriteHandler([this](const MemAccess& access) { return handleWrite(access); });
}

bool MemoryDevice::loadImage(const std::string& path, uint64_t offset) {
    if (offset >= mStorage.size()) {
        return false;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }
    input.read(reinterpret_cast<char*>(mStorage.data() + offset),
        static_cast<std::streamsize>(mStorage.size() - offset));
    return input.good() || input.eof();
}

uint64_t MemoryDevice::getSize() const {
    return mStorage.size();
}

bool MemoryDevice::isReadOnly() const {
    return mReadOnly;
}

MemResponse MemoryDevice::handleRead(const MemAccess& access) {
    if (!::isAccessValid(mStorage, access)) {
        return makeFault(access);
    }
    MemResponse response;
    response.success = true;
    response.data = readValue(mStorage, access);
    return response;
}

MemResponse MemoryDevice::handleWrite(const MemAccess& access) {
    if (!::isAccessValid(mStorage, access)) {
        return makeFault(access);
    }
    if (mReadOnly) {
        return makeFault(access);
    }
    writeValue(&mStorage, access);
    MemResponse response;
    response.success = true;
    return response;
}
