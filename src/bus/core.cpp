#include "emulator/bus.h"
#include "emulator/device.h"

#include <algorithm>

void MemoryBus::RegisterDevice(Device* device, uint64_t base, uint64_t size) {
    DeviceMapping mapping;
    mapping.DevicePtr = device;
    mapping.Base = base;
    mapping.Size = size;
    mapping.End = base + size;
    Devices.push_back(mapping);
    LastHit = nullptr;
}

const MemoryBus::DeviceMapping* MemoryBus::FindMapping(uint64_t address) const {
    if (LastHit != nullptr && address >= LastHit->Base && address < LastHit->End) {
        return LastHit;
    }
    for (const auto& mapping : Devices) {
        if (address >= mapping.Base && address < mapping.End) {
            LastHit = &mapping;
            return &mapping;
        }
    }
    return nullptr;
}

Device* MemoryBus::FindDevice(uint64_t address) const {
    const DeviceMapping* mapping = FindMapping(address);
    return mapping ? mapping->DevicePtr : nullptr;
}

MemResponse MemoryBus::Read(const MemAccess& access) {
    const DeviceMapping* mapping = FindMapping(access.Address);
    if (mapping == nullptr || mapping->DevicePtr == nullptr) {
        MemResponse response;
        response.Result = CpuResult::Error;
        response.Error.Type = CpuErrorType::AccessFault;
        response.Error.Address = access.Address;
        response.Error.Size = access.Size;
        return response;
    }
    MemAccess relativeAccess = access;
    relativeAccess.Address = access.Address - mapping->Base;
    return mapping->DevicePtr->Read(relativeAccess);
}

MemResponse MemoryBus::Write(const MemAccess& access) {
    const DeviceMapping* mapping = FindMapping(access.Address);
    if (mapping == nullptr || mapping->DevicePtr == nullptr) {
        MemResponse response;
        response.Result = CpuResult::Error;
        response.Error.Type = CpuErrorType::AccessFault;
        response.Error.Address = access.Address;
        response.Error.Size = access.Size;
        return response;
    }
    MemAccess relativeAccess = access;
    relativeAccess.Address = access.Address - mapping->Base;
    return mapping->DevicePtr->Write(relativeAccess);
}
