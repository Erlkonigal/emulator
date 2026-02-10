#ifndef EMULATOR_DEVICE_MEMORY_H
#define EMULATOR_DEVICE_MEMORY_H

#include <vector>
#include <string>
#include "emulator/device/device.h"

class MemoryDevice : public Device {
public:
    MemoryDevice(uint64_t size, bool readOnly);
    bool loadImage(const std::string& path, uint64_t offset = 0);
    uint64_t getSize() const;
    bool isReadOnly() const;

private:
    std::vector<uint8_t> mStorage;
    bool mReadOnly;

    MemResponse handleRead(const MemAccess& access);
    MemResponse handleWrite(const MemAccess& access);
};

#endif
