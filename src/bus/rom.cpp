#include "emulator/bus/rom.h"

void Rom::init(uint64_t baseAddr, const std::vector<uint8_t>& data) {
    mBaseAddr = baseAddr;
    mData = data;
}

void Rom::init(uint64_t baseAddr, const uint8_t* data, size_t size) {
    mBaseAddr = baseAddr;
    mData.assign(data, data + size);
}

bool Rom::read(uint64_t offset, uint8_t* data, size_t len) {
    if (offset + len > mData.size()) {
        size_t available = mData.size() > offset ? mData.size() - offset : 0;
        for (size_t i = 0; i < available; ++i) {
            data[i] = mData[offset + i];
        }
        for (size_t i = available; i < len; ++i) {
            data[i] = 0;
        }
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        data[i] = mData[offset + i];
    }
    return true;
}

bool Rom::write(uint64_t offset, const uint8_t* data, size_t len) {
    (void)offset;
    (void)data;
    (void)len;
    return false;
}