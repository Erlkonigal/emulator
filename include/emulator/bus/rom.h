#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

#include "emulator/bus/device.h"
#include "emulator/utils/singleton.h"

class Rom : public IDevice, public Singleton<Rom> {
public:
    void init(uint64_t baseAddr, const std::vector<uint8_t>& data);
    void init(uint64_t baseAddr, const uint8_t* data, size_t size);
    ~Rom() override = default;

    Rom(const Rom&) = delete;
    Rom& operator=(const Rom&) = delete;

    bool read(uint64_t offset, uint8_t* data, size_t len) override;
    bool write(uint64_t offset, const uint8_t* data, size_t len) override;

    uint64_t getBaseAddr() const override { return mBaseAddr; }
    uint64_t getSize() const override { return mData.size(); }
    const char* getName() const override { return "ROM"; }

private:
    uint64_t mBaseAddr = 0;
    std::vector<uint8_t> mData;

    Rom() = default;
    friend class Singleton<Rom>;
};