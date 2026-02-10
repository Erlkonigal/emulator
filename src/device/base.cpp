#include "emulator/device/device.h"

Device::Device() = default;

MemResponse Device::read(const MemAccess& access) {
    if (mReadHandler) {
        return mReadHandler(access);
    }
    MemResponse response;
    response.success = false;
    response.error.type = CpuErrorType::DeviceFault;
    response.error.address = access.address;
    response.error.size = access.size;
    return response;
}

MemResponse Device::write(const MemAccess& access) {
    if (mWriteHandler) {
        return mWriteHandler(access);
    }
    MemResponse response;
    response.success = false;
    response.error.type = CpuErrorType::DeviceFault;
    response.error.address = access.address;
    response.error.size = access.size;
    return response;
}

void Device::tick(uint64_t cycles) {
    if (mTickHandler) {
        mTickHandler(cycles);
    }
}

void Device::sync(uint64_t currentCycle) {
    if (currentCycle <= mLastSyncCycle) {
        return;
    }
    uint64_t delta = currentCycle - mLastSyncCycle;
    if (delta < mSyncThreshold) {
        return;
    }
    tick(delta);
    mLastSyncCycle = currentCycle;
}

DeviceType Device::getType() const {
    return mType;
}

void Device::setReadHandler(ReadHandler handler) {
    mReadHandler = std::move(handler);
}

void Device::setWriteHandler(WriteHandler handler) {
    mWriteHandler = std::move(handler);
}

void Device::setTickHandler(TickHandler handler) {
    mTickHandler = std::move(handler);
}

void Device::setType(DeviceType type) {
    mType = type;
}

void Device::setSyncThreshold(uint64_t threshold) {
    mSyncThreshold = threshold;
}
