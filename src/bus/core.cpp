#include "emulator/bus/bus.h"
#include "emulator/device/device.h"
#include "emulator/debugger/debugger.h"

#include <algorithm>

void MemoryBus::RegisterDevice(Device* device, uint64_t base, uint64_t size, const std::string& name) {
    for (const auto& existing : Devices) {
        if (existing.DevicePtr == device && existing.Base == base && existing.Size == size) {
            return;
        }
    }

    DeviceMapping mapping;
    mapping.Name = name;
    mapping.DevicePtr = device;
    mapping.Base = base;
    mapping.Size = size;
    mapping.End = base + size;
    Devices.push_back(mapping);
    LastHit = nullptr;

    bool found = false;
    for (auto* d : UniqueDevices) {
        if (d == device) { found = true; break; }
    }
    if (!found) {
        UniqueDevices.push_back(device);
    }
}

void MemoryBus::SyncAll(uint64_t currentCycle) {
    for (auto* device : UniqueDevices) {
        if (device) {
            device->Sync(currentCycle);
        }
    }
}

void MemoryBus::SetDebugger(Debugger* debugger) {
    Dbg = debugger;
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

Device* MemoryBus::GetDevice(const std::string& name) const {
    if (name.empty()) {
        return nullptr;
    }
    for (const auto& mapping : Devices) {
        if (mapping.Name == name) {
            return mapping.DevicePtr;
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
        response.Success = false;
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
        response.Success = false;
        response.Error.Type = CpuErrorType::AccessFault;
        response.Error.Address = access.Address;
        response.Error.Size = access.Size;
        return response;
    }

    MemAccess relativeAccess = access;
    relativeAccess.Address = access.Address - mapping->Base;
    return mapping->DevicePtr->Write(relativeAccess);
}
