#ifndef EMULATOR_BUS_BUS_H
#define EMULATOR_BUS_BUS_H

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "emulator/cpu/cpu.h"

class Device;
class Debugger;

struct MemoryRegion {
    const char* name = nullptr;
    uint64_t base = 0;
    uint64_t size = 0;
};

inline bool computeRegionEnd(uint64_t base, uint64_t size, uint64_t* end) {
    if (end == nullptr || size == 0) {
        return false;
    }
    if (base > std::numeric_limits<uint64_t>::max() - size) {
        return false;
    }
    *end = base + size;
    return true;
}

inline bool regionsOverlap(const MemoryRegion& a, const MemoryRegion& b) {
    uint64_t endA = 0;
    uint64_t endB = 0;
    if (!computeRegionEnd(a.base, a.size, &endA) || !computeRegionEnd(b.base, b.size, &endB)) {
        return true;
    }
    return a.base < endB && b.base < endA;
}

inline bool validateMappings(const std::vector<MemoryRegion>& mappings, std::string* error) {
    for (const auto& mapping : mappings) {
        uint64_t end = 0;
        if (!computeRegionEnd(mapping.base, mapping.size, &end)) {
            if (error != nullptr) {
                *error = std::string("Invalid mapping: ") + mapping.name;
            }
            return false;
        }
    }
    for (size_t i = 0; i < mappings.size(); ++i) {
        for (size_t j = i + 1; j < mappings.size(); ++j) {
            if (regionsOverlap(mappings[i], mappings[j])) {
                if (error != nullptr) {
                    *error = std::string("Overlapping mappings: ") + mappings[i].name +
                        " and " + mappings[j].name;
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

    void registerDevice(Device* device, uint64_t base, uint64_t size, const std::string& name = "");
    Device* findDevice(uint64_t address) const;
    Device* getDevice(const std::string& name) const;
    MemResponse read(const MemAccess& access);
    MemResponse write(const MemAccess& access);
    void syncAll(uint64_t currentCycle);
    void setDebugger(Debugger* debugger);

    const std::vector<Device*>& getDevices() const { return mUniqueDevices; }

private:
    struct DeviceMapping {
        std::string name;
        Device* devicePtr = nullptr;
        uint64_t base = 0;
        uint64_t size = 0;
        uint64_t end = 0;
    };

    const DeviceMapping* findMapping(uint64_t address) const;

    std::vector<DeviceMapping> mDevices;
    std::vector<Device*> mUniqueDevices;
    mutable const DeviceMapping* mLastHit = nullptr;
    Debugger* mDbg = nullptr;
};

#endif
