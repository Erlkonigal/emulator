#include "emulator/bus/ram.h"

void Ram::init(uint64_t baseAddr, uint64_t size) {
    mBaseAddr = baseAddr;
    mData.assign(size, 0);
}

bool Ram::read(uint64_t offset, uint8_t* data, size_t len) {
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

bool Ram::write(uint64_t offset, const uint8_t* data, size_t len) {
    if (offset + len > mData.size()) {
        size_t available = mData.size() > offset ? mData.size() - offset : 0;
        for (size_t i = 0; i < available; ++i) {
            mData[offset + i] = data[i];
        }
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        mData[offset + i] = data[i];
    }
    return true;
}