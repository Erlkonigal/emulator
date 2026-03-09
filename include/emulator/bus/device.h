#pragma once

#include <cstdint>
#include <cstddef>

class IDevice {
public:
    virtual ~IDevice() = default;

    IDevice(const IDevice&) = delete;
    IDevice& operator=(const IDevice&) = delete;

    virtual bool read(uint64_t offset, uint8_t* data, size_t len) = 0;
    virtual bool write(uint64_t offset, const uint8_t* data, size_t len) = 0;

    virtual uint64_t getBaseAddr() const = 0;
    virtual uint64_t getSize() const = 0;
    virtual const char* getName() const = 0;

protected:
    IDevice() = default;
};