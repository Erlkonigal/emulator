#include "emulator/device/rom.h"

#include <algorithm>
#include <cstring>

void Rom::init(const std::vector<uint8_t>& data) {
    mData = data;
}

void Rom::init(const uint8_t* data, size_t size) {
    mData.assign(data, data + size);
}

void Rom::reset() {
    std::fill(mData.begin(), mData.end(), 0);
}

bool Rom::read(uint64_t offset, uint8_t* data, size_t len) {
    if (offset >= mData.size()) {
        std::memset(data, 0, len);
        return false;
    }
    
    size_t available = mData.size() - offset;
    if (available >= len) {
        std::memcpy(data, mData.data() + offset, len);
        return true;
    }
    
    std::memcpy(data, mData.data() + offset, available);
    std::memset(data + available, 0, len - available);
    return false;
}

bool Rom::write(uint64_t offset, const uint8_t* data, size_t len) {
    (void)offset;
    (void)data;
    (void)len;
    return false;
}