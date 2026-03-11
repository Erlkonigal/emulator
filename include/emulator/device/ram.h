#pragma once

#include <cstdint>
#include <cstddef>

#include "emulator/device/device.h"
#include "emulator/utils/singleton.h"

class Ram : public Singleton<Ram>, public IDevice {
public:
    void init(uint64_t size);
    ~Ram();

    bool read(uint64_t offset, uint8_t* data, size_t len) override;
    bool write(uint64_t offset, const uint8_t* data, size_t len) override;

    void reset() override;
    uint64_t getSize() const { return mSize; }

private:
    uint8_t* mData = nullptr;
    uint64_t mSize = 0;

    Ram() = default;
    friend class Singleton<Ram>;
};