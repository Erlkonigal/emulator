#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

#include "emulator/bus/device.h"
#include "emulator/utils/singleton.h"

class Bus : public Singleton<Bus> {
public:
    void registerDevice(IDevice* device);
    void unregisterDevice(IDevice* device);
    void clear();

    bool read(uint64_t addr, uint8_t* data, size_t len);
    bool write(uint64_t addr, const uint8_t* data, size_t len);

    uint8_t read8(uint64_t addr);
    uint16_t read16(uint64_t addr);
    uint32_t read32(uint64_t addr);
    uint64_t read64(uint64_t addr);

    void write8(uint64_t addr, uint8_t data);
    void write16(uint64_t addr, uint16_t data);
    void write32(uint64_t addr, uint32_t data);
    void write64(uint64_t addr, uint64_t data);

private:
    std::vector<IDevice*> mDevices;

    IDevice* findDevice(uint64_t addr, uint64_t& offset);

    Bus() = default;
    friend class Singleton<Bus>;
};