#pragma once

#include <cstdint>
#include <cstddef>

class IDevice {
public:
    virtual ~IDevice() = default;

    virtual bool read(uint64_t offset, uint8_t* data, size_t len) = 0;
    virtual bool write(uint64_t offset, const uint8_t* data, size_t len) = 0;
    virtual void reset() = 0;
};