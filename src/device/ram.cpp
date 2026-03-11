#include "emulator/device/ram.h"

#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>

void Ram::init(uint64_t size) {
    if (mData != nullptr) {
        munmap(mData, mSize);
    }
    mSize = size;
    mData = static_cast<uint8_t*>(mmap(
        nullptr, size, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0));
    if (mData == MAP_FAILED) {
        mData = nullptr;
        mSize = 0;
    }
}

Ram::~Ram() {
    if (mData != nullptr) {
        munmap(mData, mSize);
        mData = nullptr;
        mSize = 0;
    }
}

void Ram::reset() {
    if (mData != nullptr && mSize > 0) {
        madvise(mData, mSize, MADV_DONTNEED);
    }
}

bool Ram::read(uint64_t offset, uint8_t* data, size_t len) {
    if (mData == nullptr || offset >= mSize) {
        std::memset(data, 0, len);
        return false;
    }
    size_t available = mSize - offset;
    if (available >= len) {
        std::memcpy(data, mData + offset, len);
    } else {
        std::memcpy(data, mData + offset, available);
        std::memset(data + available, 0, len - available);
        return false;
    }
    return true;
}

bool Ram::write(uint64_t offset, const uint8_t* data, size_t len) {
    if (mData == nullptr || offset >= mSize) {
        return false;
    }
    size_t available = mSize - offset;
    if (available >= len) {
        std::memcpy(mData + offset, data, len);
    } else {
        std::memcpy(mData + offset, data, available);
        return false;
    }
    return true;
}