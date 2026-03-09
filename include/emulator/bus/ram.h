#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

#include "emulator/bus/device.h"
#include "emulator/utils/singleton.h"

class Ram : public IDevice, public Singleton<Ram> {
public:
    void init(uint64_t baseAddr, uint64_t size);
    ~Ram() override = default;

    Ram(const Ram&) = delete;
    Ram& operator=(const Ram&) = delete;

    bool read(uint64_t offset, uint8_t* data, size_t len) override;
    bool write(uint64_t offset, const uint8_t* data, size_t len) override;

    uint64_t getBaseAddr() const override { return mBaseAddr; }
    uint64_t getSize() const override { return mData.size(); }
    const char* getName() const override { return "RAM"; }

private:
    uint64_t mBaseAddr = 0;
    std::vector<uint8_t> mData;

    Ram() = default;
    friend class Singleton<Ram>;
};