#include "emulator/bus/bus.h"
#include "emulator/device/device.h"
#include "emulator/debugger/debugger.h"

#include <algorithm>

void MemoryBus::registerDevice(Device* device, uint64_t base, uint64_t size, const std::string& name) {
    for (const auto& existing : mDevices) {
        if (existing.devicePtr == device && existing.base == base && existing.size == size) {
            return;
        }
    }

    DeviceMapping mapping;
    mapping.name = name;
    mapping.devicePtr = device;
    mapping.base = base;
    mapping.size = size;
    mapping.end = base + size;
    mDevices.push_back(mapping);
    mLastHit = nullptr;

    bool found = false;
    for (auto* d : mUniqueDevices) {
        if (d == device) { found = true; break; }
    }
    if (!found) {
        mUniqueDevices.push_back(device);
    }
}

void MemoryBus::syncAll(uint64_t currentCycle) {
    for (auto* device : mUniqueDevices) {
        if (device) {
            device->sync(currentCycle);
        }
    }
}

void MemoryBus::setDebugger(Debugger* debugger) {
    mDbg = debugger;
}

const MemoryBus::DeviceMapping* MemoryBus::findMapping(uint64_t address) const {
    if (mLastHit != nullptr && address >= mLastHit->base && address < mLastHit->end) {
        return mLastHit;
    }
    for (const auto& mapping : mDevices) {
        if (address >= mapping.base && address < mapping.end) {
            mLastHit = &mapping;
            return &mapping;
        }
    }
    return nullptr;
}

Device* MemoryBus::getDevice(const std::string& name) const {
    if (name.empty()) {
        return nullptr;
    }
    for (const auto& mapping : mDevices) {
        if (mapping.name == name) {
            return mapping.devicePtr;
        }
    }
    return nullptr;
}

Device* MemoryBus::findDevice(uint64_t address) const {
    const DeviceMapping* mapping = findMapping(address);
    return mapping ? mapping->devicePtr : nullptr;
}

MemResponse MemoryBus::read(const MemAccess& access) {
    const DeviceMapping* mapping = findMapping(access.address);
    if (mapping == nullptr || mapping->devicePtr == nullptr) {
        MemResponse response;
        response.success = false;
        response.error.type = CpuErrorType::AccessFault;
        response.error.address = access.address;
        response.error.size = access.size;
        return response;
    }

    MemAccess relativeAccess = access;
    relativeAccess.address = access.address - mapping->base;
    return mapping->devicePtr->read(relativeAccess);
}

MemResponse MemoryBus::write(const MemAccess& access) {
    const DeviceMapping* mapping = findMapping(access.address);
    if (mapping == nullptr || mapping->devicePtr == nullptr) {
        MemResponse response;
        response.success = false;
        response.error.type = CpuErrorType::AccessFault;
        response.error.address = access.address;
        response.error.size = access.size;
        return response;
    }

    MemAccess relativeAccess = access;
    relativeAccess.address = access.address - mapping->base;
    return mapping->devicePtr->write(relativeAccess);
}
