#include "emulator/bus/bus.h"
#include "emulator/log/logger.h"

#include <algorithm>

void Bus::registerDevice(IDevice* device) {
    if (device == nullptr) {
        return;
    }
    auto it = std::find(mDevices.begin(), mDevices.end(), device);
    if (it == mDevices.end()) {
        mDevices.push_back(device);
        DEBUG("Bus: registered device '%s' at 0x%llx, size 0x%llx",
              device->getName(),
              (unsigned long long)device->getBaseAddr(),
              (unsigned long long)device->getSize());
    }
}

void Bus::unregisterDevice(IDevice* device) {
    auto it = std::find(mDevices.begin(), mDevices.end(), device);
    if (it != mDevices.end()) {
        mDevices.erase(it);
    }
}

void Bus::clear() {
    mDevices.clear();
}

IDevice* Bus::findDevice(uint64_t addr, uint64_t& offset) {
    for (auto* device : mDevices) {
        uint64_t base = device->getBaseAddr();
        uint64_t size = device->getSize();
        if (addr >= base && addr < base + size) {
            offset = addr - base;
            return device;
        }
    }
    return nullptr;
}

bool Bus::read(uint64_t addr, uint8_t* data, size_t len) {
    uint64_t offset = 0;
    IDevice* device = findDevice(addr, offset);
    if (device == nullptr) {
        WARN("Bus: read from unmapped address 0x%llx", (unsigned long long)addr);
        std::fill(data, data + len, 0);
        return false;
    }
    return device->read(offset, data, len);
}

bool Bus::write(uint64_t addr, const uint8_t* data, size_t len) {
    uint64_t offset = 0;
    IDevice* device = findDevice(addr, offset);
    if (device == nullptr) {
        WARN("Bus: write to unmapped address 0x%llx", (unsigned long long)addr);
        return false;
    }
    return device->write(offset, data, len);
}

uint8_t Bus::read8(uint64_t addr) {
    uint8_t data = 0;
    read(addr, &data, 1);
    return data;
}

uint16_t Bus::read16(uint64_t addr) {
    uint16_t data = 0;
    read(addr, reinterpret_cast<uint8_t*>(&data), 2);
    return data;
}

uint32_t Bus::read32(uint64_t addr) {
    uint32_t data = 0;
    read(addr, reinterpret_cast<uint8_t*>(&data), 4);
    return data;
}

uint64_t Bus::read64(uint64_t addr) {
    uint64_t data = 0;
    read(addr, reinterpret_cast<uint8_t*>(&data), 8);
    return data;
}

void Bus::write8(uint64_t addr, uint8_t data) {
    write(addr, &data, 1);
}

void Bus::write16(uint64_t addr, uint16_t data) {
    write(addr, reinterpret_cast<uint8_t*>(&data), 2);
}

void Bus::write32(uint64_t addr, uint32_t data) {
    write(addr, reinterpret_cast<uint8_t*>(&data), 4);
}

void Bus::write64(uint64_t addr, uint64_t data) {
    write(addr, reinterpret_cast<uint8_t*>(&data), 8);
}