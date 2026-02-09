#ifndef EMULATOR_DEVICE_MEMORY_H
#define EMULATOR_DEVICE_MEMORY_H

#include <vector>
#include <string>
#include "emulator/device/device.h"

class MemoryDevice : public Device {
public:
    MemoryDevice(uint64_t size, bool readOnly);
    bool LoadImage(const std::string& path, uint64_t offset = 0);
    uint64_t GetSize() const;
    bool IsReadOnly() const;

private:
    std::vector<uint8_t> Storage;
    bool ReadOnly;

    MemResponse HandleRead(const MemAccess& access);
    MemResponse HandleWrite(const MemAccess& access);
};

#endif
