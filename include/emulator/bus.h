#ifndef EMULATOR_BUS_H
#define EMULATOR_BUS_H

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "emulator/cpu.h"

class Device;

struct MemoryRegion {
    const char* Name = nullptr;
    uint64_t Base = 0;
    uint64_t Size = 0;
};

inline bool ComputeRegionEnd(uint64_t base, uint64_t size, uint64_t* end) {
    if (end == nullptr || size == 0) {
        return false;
    }
    if (base > std::numeric_limits<uint64_t>::max() - size) {
        return false;
    }
    *end = base + size;
    return true;
}

inline bool RegionsOverlap(const MemoryRegion& a, const MemoryRegion& b) {
    uint64_t endA = 0;
    uint64_t endB = 0;
    if (!ComputeRegionEnd(a.Base, a.Size, &endA) || !ComputeRegionEnd(b.Base, b.Size, &endB)) {
        return true;
    }
    return a.Base < endB && b.Base < endA;
}

inline bool ValidateMappings(const std::vector<MemoryRegion>& mappings, std::string* error) {
    for (const auto& mapping : mappings) {
        uint64_t end = 0;
        if (!ComputeRegionEnd(mapping.Base, mapping.Size, &end)) {
            if (error != nullptr) {
                *error = std::string("Invalid mapping: ") + mapping.Name;
            }
            return false;
        }
    }
    for (size_t i = 0; i < mappings.size(); ++i) {
        for (size_t j = i + 1; j < mappings.size(); ++j) {
            if (RegionsOverlap(mappings[i], mappings[j])) {
                if (error != nullptr) {
                    *error = std::string("Overlapping mappings: ") + mappings[i].Name +
                        " and " + mappings[j].Name;
                }
                return false;
            }
        }
    }
    return true;
}

class MemoryBus {
public:
    MemoryBus() = default;

    void RegisterDevice(Device* device, uint64_t base, uint64_t size);
    Device* FindDevice(uint64_t address) const;
    MemResponse Read(const MemAccess& access);
    MemResponse Write(const MemAccess& access);

private:
    struct DeviceMapping {
        Device* DevicePtr = nullptr;
        uint64_t Base = 0;
        uint64_t Size = 0;
        uint64_t End = 0;
    };

    const DeviceMapping* FindMapping(uint64_t address) const;

    std::vector<DeviceMapping> Devices;
    mutable const DeviceMapping* LastHit = nullptr;
};

#endif
